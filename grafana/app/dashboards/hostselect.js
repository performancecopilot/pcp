/* global _ */

/*
 * Complex scripted dashboard
 * This script generates a dashboard object that Grafana can load. It also takes a number of user
 * supplied URL parameters (int ARGS variable)
 *
 * Global accessable variables
 * window, document, $, jQuery, ARGS, moment
 *
 * Return a dashboard object, or a function
 *
 * For async scripts, return a function, this function must take a single callback function,
 * call this function with the dasboard object
 */

// accessable variables in this scope
var window, document, ARGS, $, jQuery, moment, kbn;


// normalize the input parameters
function arrayize(field, defaults) {
    var array;
    var value = ARGS[field];
    if (value == null) { // might be a missing queryparam
        array = defaults;
    } else if (! $.isArray(value)) { // might be a scalar value if singleton queryparam
        array = [value];
    } else { // might be array if repeated queryparam
        array = value;
    }
    return array;
}

// normalize the input parameters, fill in defaults
var metrics = arrayize("metric", [
    // roughly the set of the hard-coded default.json dashboard
    "kernel.all.load.1 minute",
    "network.interface.in.bytes.*",
    "network.interface.out.bytes.*",
    "disk.dev.read_bytes.*",
    "disk.dev.write_bytes.*",
    "mem.util.available",
    "filesys.full.*"
]);
var titles = arrayize("title", new Array(metrics.length));
var styles = arrayize("style", new Array(metrics.length));
var heights = arrayize("height", new Array(metrics.length));

// time bounds, will be passed along to multichart per-host views
var from = ARGS["from"]; if (from == null) { from = "now-2d"; }
var to = ARGS["to"];     if (to == null)   { to = "now"; }


// XXX: other ideas:
// a URL parameter to regex-filter hostnames
// a URL parameter to flag whether containers should be in/excluded


// assemble a url querystring suffix, passing along the incoming
// metrics/titles/styles/heights to a call to multichart.js, and
// substituting the target_prefix into each metric
function assemble_multichart_url(target_prefix,dispname) {
    var suffix="";
    suffix = suffix + "&from=" + encodeURIComponent(from);
    suffix = suffix + "&to=" + encodeURIComponent(to);
    var def_style = (target_prefix=="" ? "png" : "json");
    var def_height = "250";
    for (var i=0; i<metrics.length; i++) {
        suffix = suffix + "&target=" + encodeURIComponent(target_prefix+"*."+metrics[i]);
        suffix = suffix + "&title=" + (titles[i]==null ? encodeURIComponent(metrics[i]) : encodeURIComponent(titles[i]));
        suffix = suffix + "&style=" + (styles[i]==null ? def_style : encodeURIComponent(styles[i]));
        suffix = suffix + "&height=" + (heights[i]==null ? def_height : encodeURIComponent(heights[i]));
    }
    suffix = suffix + "&name=" + encodeURIComponent(dispname);
    return suffix;
}


// Demangle the hostname part of a pmwebd graphite archive-path string.
// Should match pmwebd's pmgraphite_metric_decode() routine, though
// this copy is only for aesthetic (human reading) purposes, so can
// let glitches through.
function pmwebd_demangle(X,str) {
    // http://stackoverflow.com/a/4209150/661150
    if (X) {
        return str.replace(/~([0-9A-F][0-9A-F])/g, function() {
            return String.fromCharCode(parseInt(arguments[1], 16));
        });
    } else {
        return str.replace(/-([0-9A-F][0-9A-F])-/g, function() {
            return String.fromCharCode(parseInt(arguments[1], 16));
        });
    }
}


return function(callback) {
  // Setup some variables
  var dashboard;
    
    // Intialize a skeleton with nothing but a rows array and service object
    dashboard = {
        style: 'light',
        rows: [],
        services : {},
        nav : { enable: false }
    };
    
    // Set a title
    dashboard.title = 'PCP+Grafana host list';
    
    // Set default time
    // time can be overriden in the url using from/to parameteres, but this is
    // handled automatically in grafana core during dashboard initialization
    dashboard.time = {
        from: "now-6h",
        to: "now"
    };
    
    // Fetch a dump of the pmcd.pmlogger.port metric that pmlogger
    // always puts at the front of its archives.  From the resulting
    // data, we'll figure out what hosts have archives available.

    var pmwebd = location.protocol + "//" + location.hostname + ":" + location.port;
    var multichart = pmwebd + "/grafana/index.html#/dashboard/script/multichart.js?";

    $.ajax({
        method: 'GET',
        url: pmwebd + "/graphite/render?format=json&target=*.pmcd.pmlogger.port.*&from=-1m&until=now"
    })
        .done(function(result) {
            // console.log(result);
            var hosts = {}; // map from hostname to array-of-containers
            var pmwebd_X_mode = false;
            for (var i=0; i<result.length; i++) {
                var target = result[i].target.split(".")[0];
                if(target.match(/[\/~]/)) { // guess at -X mode being on or off by presence of special chars
                    pmwebd_X_mode = true;
                }

                var hostname, host_basename, host_contname;
                if (pmwebd_X_mode) {
                    hostname = target.replace(/\/(archive|reduced).*$/, "");
                    host_basename = hostname.replace(/--.*$/,"");
                    host_contname = hostname.replace(/^.*--/,"");
                } else {
                    hostname = target.replace(/-2F-(archive|reduced).*$/, "");
                    host_basename = hostname.replace(/-2D--2D-.*$/,"");
                    host_contname = hostname.replace(/^.*-2D--2D-/,"");
                }
                if (! hosts.hasOwnProperty(host_basename)) { // new host?
                    hosts[host_basename]={};
                }
                if (hostname != host_basename) { // has container?
                    hosts[host_basename][host_contname] = true;
                }
            }
            //console.log(hosts);
            var markdown = "";

            markdown = "### metrics ...\n\n";
            for (m in metrics) {
                markdown = markdown + "* " + metrics[m] + "\n";
            }
            markdown = markdown + "\n\n";

            markdown = markdown + "### ... on all hosts (png graphs)\n";
            markdown = markdown + "* [all hosts]("+multichart+assemble_multichart_url("","all hosts")+")";
            markdown = markdown + "\n\n";
            
            var target_suffix = ".proc.nprocs";
            var last_base_hostname = "";
            markdown = markdown + "\n### ... on individual hosts (flot graphs)\n\n";
            for (var hostname in hosts) {
                if (hosts.hasOwnProperty(hostname)) {
                    hostdispname = pmwebd_demangle(pmwebd_X_mode, hostname);
                    markdown = markdown + "* ["+hostdispname+"]("+multichart+assemble_multichart_url(hostname,hostdispname)+")";
                    // iterate over its containers
                    var printed = false;
                    for (var contname in hosts[hostname]) {
                        if (! printed) {
                            printed=true;
                            markdown = markdown + " containers: ";
                        }
                        var contdispname = pmwebd_demangle(pmwebd_X_mode, contname.substring(0,6)+"..."); // abbreviate it
                        var conthostname = hostname + (pmwebd_X_mode ? "--" : "-2D--2D-") + contname;
                        markdown = markdown + " ["+contdispname+"]"+
                            "("+multichart+assemble_multichart_url(conthostname,hostdispname+" container "+contdispname)+")";
                    }
                    markdown = markdown + "\n";
                }
            }
            //console.log(markdown);
            
    dashboard.rows.push({
        title: 'Hostname list',
        // height: '600px', // unneeded - autoscales
        panels: [
            {
                title: 'View metrics on selected host',
                type: 'text',
                span: 12,
                fill: 1,
                content: markdown
            }
        ]
    });

    // when dashboard is composed call the callback
    // function and pass the dashboard
    callback(dashboard);
  });
}


