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
var styles = arrayize("style", []);
var heights = arrayize("heights", []);
// XXX: other ideas
// a parameter to select overall dashboard color theme
var name = ARGS["name"]; if (name == null) { name = "PCP+Grafana charts"; }

dashboard.title = name;

console.log(targets);

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
    if (TITLE == null) TITLE = "";

    var STYLE = styles[i];
    if (STYLE == null || STYLE != "png") STYLE = "flot"; // default "flot"

    var HEIGHT = heights[i];
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

    var SUBTARGETS = TARGET.split(',');
    for (j in SUBTARGETS) {
        panel.targets.push({target: SUBTARGETS[j]});
    }
    
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

return dashboard;
