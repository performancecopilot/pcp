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

var pmwebd = location.protocol + "//" + location.hostname + ":" + location.port;
var checklist_url = "/grafana/index.html#/dashboard/script/checklist.js";

return function(callback) {
    $.ajax({
        method: 'GET',
        url: pmwebd + "/checklist/checklist.json"
    }).done(function(result) {
        var nodes = result.nodes;
        var node = ARGS["node"];
        var dashboard;
	var node_map = [];
        var panel;

	// create a table mapping nodes names to indices
        for (var i=0; i<nodes.length; i++) {
	    node_map[nodes[i].name] = i;
	}

	// FIXME debug code
        for (t in node_map) {
	    console.log(t, "=", node_map[t]);
	}

        // url parameter to identify checklist node
        if (node == undefined) { node = "Start"; }
        
        // find
	if (node in node_map) {
            panel = nodes[node_map[node]];
	} else {
            alert ("checklist node " + node + "not found");
            callback(dashboard);
            return;
        }

        // Intialize a skeleton with nothing but a rows array and service object
        dashboard = {
            style: 'light',
            rows: [],
            services : {},
            refresh: '2s', // XXX: parametrize
            style: 'light',
            nav: [ { type : "timepicker",
                     collapse: false,
                     enable : true,
                     time_options : [ "5m", "15m", "1h", "6h",
                                      "12h", "24h", "2d", "7d",
                                      "30d", "90d", "365d" ],
                     now: true
                   } ],
            time: { from: "now-1m", // overriden with ARGS[from]
                    to: "now"       // overriden with ARGS[to]
                  }
        };
        
        // Set a title
        dashboard.title = 'PCP CHECKLIST ' + node;
        
        // create navigation link/menu at the top
        // XXX: grafana suppresses normal click linkfollowing action in these A HREF links
        var markdown = "";
        markdown = "[**RESTART**](" + pmwebd + checklist_url + ")";
        for (var i=0; i<nodes.length; i++) {
            var highlight = "";
            if (nodes[i].name == node) {
                highlight="**";
            }
            markdown +=
                " | [" + highlight + nodes[i].name + highlight + "]" +
                "(" + pmwebd + checklist_url + "?node=" + encodeURIComponent(nodes[i].name) + ")";
        }
        dashboard.rows.push({
            title: '', height: '0',
            panels: [ { title: 'Navigation', type: 'text', span: 12, fill: 1, content: markdown } ]
        });

        // create description panel
        var markdown = "";
        if ("description" in panel) {
            markdown = panel.description;
        } else {
            markdown = "(no description)";
        }
        dashboard.rows.push({
            title: '', height: '0',
            panels: [ { title: 'Description', type: 'text', span: 12, fill: 1, content: markdown } ]
        });

        // create pcp metrics panel
        if ("pcp_metrics" in panel) {
            var metrics = panel.pcp_metrics;
            console.log(metrics);

            if (! (metrics instanceof Array))
                metrics = [ metrics ];
            for (t in metrics) {
                var targets = metrics[t];
                var panel = {
                    title: targets,
                    type: 'graph',
                    span: 12,
                    fill: 1,
                    linewidth: 2,
                    targets: [],
                    seriesOverrides: [ { yaxis: 2, fill: 0, linewidth: 5 } ]
                };
                var subtargets = targets.split(',');
                for (j in subtargets) {
                    panel.targets.push({target: "*."+subtargets[j]});
                }

                console.log(panel);
                dashboard.rows.push({
                    title: 'Chart',
                    height: 250, // XXX: param
                    panels: [ panel ]});
            }
        }
        
        // when dashboard is composed call the callback function and
        // pass the dashboard
        callback(dashboard);
    });
}
