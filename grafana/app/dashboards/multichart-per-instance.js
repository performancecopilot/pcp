/* global _ */
/*
 * Generates graphs for each instance
 * Parameters:
 *   target: Base target. example: pmmgr-*HOSTNAME*.disk.dm
 *   subTarget: Each subtarget will be displayed as a line inside each graph. example: read_bytes,write_bytes
 *   filter: Used to filter instance names. example: vg* to only list devices starting with vg
 *
 */

// accessable variables in this scope
var window, document, ARGS, $, jQuery, moment, kbn;

// All url parameters are available via the ARGS object
var ARGS;

// Intialize a skeleton with nothing but a rows array and service object
var dashboard = {
    rows : [],
    timezone: 'utc',
    refresh: '5m',
    style: 'light',
    nav: [ { type : "timepicker",
             collapse: false,
             enable : true,
             time_options : [ "5m", "15m", "1h", "6h",
                              "12h", "24h", "2d", "7d",
                              "30d", "90d", "365d" ],
             now: true
           } ],
    time: { from: "now-2d", // overriden with ARGS[from]
            to: "now"       // overriden with ARGS[to]
          }
};

// Adds a new panel
function addPanel(baseTarget, subTargets, instance) {

    var TITLE = subTargets.join(",") + " for " + instance;
    if (TITLE == null) TITLE = instance;

    var STYLE = style;
    if (STYLE == null || STYLE != "png") STYLE = "flot"; // default "flot"

    var HEIGHT = height;
    if (HEIGHT == null) HEIGHT = "250px";

    var panel = {
        title: TITLE,
        type: 'graph',
        span: 12,
        fill: 1,
        linewidth: 2,
        targets: [],
        seriesOverrides: [ {
            yaxis: 2,
            fill: 0,
            linewidth: 5
        } ]
    };

    subTargets.forEach(function(e) {
        panel.targets.push({target: baseTarget + "." + e + "." + instance});
    }); 
    
    if (STYLE == "png") {
        panel.renderer = "png";
        panel.legend = true;
    } else {
        panel.legend = false;
    }

    dashboard.rows.push({
        title: 'Chart',
        height: HEIGHT,
        panels: [ panel ]});
}

function generatePanels(result) {
    console.log(result);

    var instances = result.map(function(item) {
        var target = item.target.split(".");
        return target[target.length - 1];
    })

    instances = [...new Set(instances)];

    instances.forEach(function(e) {
        addPanel(baseTarget,subTargets, e);
    });
}

function getInstances(baseTarget,subTargets,filter) {
   var pmwebd = location.protocol + "//" + location.hostname + ":" + location.port;

   // TODO: It will only get the interfaces of first subTarget
   var searchTarget = baseTarget + "." + subTargets[0] + "." + filter;

   // TODO: Fix this
   //var from = ARGS["from"];
   //var to = ARGS["to"];
   $.ajax({
        method: 'GET',
        url: pmwebd + "/graphite/render?format=json&target=" + searchTarget + "&from=-10m&until=now"
    }).done(function(result) {
       generatePanels(result);
    })
}

var name = ARGS["name"]; if (name == null) { name = "PCP+Grafana charts"; }
var baseTarget = ARGS["target"];
var subTargets = ARGS["subTarget"].split(",");
var filter = ARGS["filter"]; if (filter == null) { filter = "*"; }
var theme = ARGS["theme"]; if (theme == null) { theme = "light" }
var style = ARGS["style"];
var height = ARGS["height"];

dashboard.title = name;
dashboard.style = theme;

getInstances(baseTarget,subTargets,filter);

return dashboard;
