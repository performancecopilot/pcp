/* global _ */

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

var targets = arrayize("target", [null]);
var titles = arrayize("title", []);
var style = ARGS["style"]; if (style == null) { style = "flot"; }
var height;
var heights = ARGS["height"]; if (heights == null) { height = 200; } else { height = parseInt(heights); }

// XXX: other ideas
// a parameter to select overall dashboard color theme
var name = ARGS["name"]; if (name == null) { name = "PCP+Grafana charts"; }
var template = ARGS["template"];
var template_p = true;
if (template == null) { template_p = false; }
var span12s = ARGS["span12s"]; if (span12s == null) { span12 = 4; } else { span12=parseInt(span12s); }

if (template_p) {
    dashboard.templating = {
        enable: true,
        list: [ { datasource: null,
                  includeAll: true,
                  name: "host", /* -> $host elsewhere */
                  query: template,
                  refresh_on_load: true,
                  refresh: true,
                  type: 'query',
                  regex: '/(.*)/',
                  allFormat: 'wildcard',
                  options: [ { text: 'All', value: '*' } ],
                  current: { text: 'All', value: '*' }
                } ] };
}

dashboard.title = name;

// console.log(targets);

var totalspan = 0;
var panels = [];
for (var i = 0; i<targets.length; i++) {
    var TARGET = targets[i];
    if (TARGET == null || TARGET=="") {
        dashboard.rows.push({
            title: 'Error',
            panels: [ { title: 'Error', type: 'text', span: 12, content: 'missing target=TARGET url parameter'} ]
        });
        continue;
    }
    
    var TITLE = titles[i];
    if (TITLE == null) TITLE = TARGET.replace(',',' '); // can get wide :(

    var panel = {
        title: TITLE,
        type: 'graph',
        span: span12,
        fill: 1,
        linewidth: 2,
        nullPointMode: "null", /* "connected" ? */
        /*        tooltip: { shared: true }, */
        targets: [],
        seriesOverrides: [ {
            yaxis: 2,
            fill: 0,
            linewidth: 5
        } ]
    };
    if (style == "png") {
        panel.renderer = "png";
        panel.legend = true;
    } else {
        panel.legend = false;
    }

    var SUBTARGETS = TARGET.split(',');
    for (j in SUBTARGETS) {
        panel.targets.push({target: (template_p ? "$host."+SUBTARGETS[j] : SUBTARGETS[j])});
    }

    /* Shift panels onto a row when we fill up the twelve 12ths, or 
       we've just processed the last panel. */
    panels.push(panel);
    totalspan = totalspan + span12;
    
    if (span12 > 12 || (i+1 == targets.length)) {
        dashboard.rows.push({
            title: 'Chart',
            height: height,
            panels: panels});
        totalspan = 0;
        panels = [];
    }
}

return dashboard;
