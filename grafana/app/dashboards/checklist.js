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
var pmwebapi_url = "/pmapi/1/"; // context 1 = hardcoded to local: via checklist.sh
var checklist_url = "/grafana/index.html#/dashboard/script/checklist.js";

function create_metric_panel(dashboard, node) {
    var metrics = node.pcp_metrics;
    console.log(metrics);

    if (!(metrics instanceof Array))
        metrics = [metrics];
    for (t in metrics) {
        var targets = metrics[t];
        var panel = {
            title: targets,
            type: 'graph',
            span: 12,
            fill: 1,
            linewidth: 2,
            targets: [],
            seriesOverrides: [{
                yaxis: 2,
                fill: 0,
                linewidth: 5
            }]
        };
        var subtargets = targets.split(',');
        for (j in subtargets) {
            panel.targets.push({
                target: "*." + subtargets[j]
            });
        }

        dashboard.rows.push({
            title: 'Chart',
            height: 250, // XXX: param
            panels: [panel]
        });
    }
}


/* We install some functions (lambdas) in the top. object, for
   use from the html DOM in the grafana text panels.  They can't
   call functions defined ordinary in this .js file, since it
   is loaded specially, not as a <script ...> for the top level. */

/* Fetch current values of all the metrics needed by any predicate we
   know of: namely the top.rhck_predicate_metrics list, and store them
   in the top.rhck_predicate_metrics map. */
function checklist_predicate_fetch() {
    console.log("FETCHING ", top.rhck_predicate_metric_names);
    if (top.rhck_predicate_metric_names.length > 0) {
        var unique_names = [];
        var pmwebapi_url = top.rhck_predicate_pmwebapi + "_fetch?names=";
        $.each(top.rhck_predicate_metric_names, function (i, name) {
            if ($.inArray(name, unique_names) === -1) {
                unique_names.push(name);
                if (i > 0) pmwebapi_url += ",";
                pmwebapi_url += encodeURIComponent(name);
            }
        });
        $.getJSON(pmwebapi_url, function (data, status) {
            var data_dict = {}; // { "metric": { "pmid":... "name":... "instances":[{}] }
            try {
                $.each(data.values, function (i, value) {
                    data_dict[value.name] = value;
                });
                $.each(top.rhck_predicate_notifiers, function (i, notify) {
                    try {
                        notify(data_dict);
                    } finally {}
                });
            } finally {}
        });
    }
    setTimeout(checklist_predicate_fetch, top.rhck_predicate_fetch_every);
}


// javascript code generator for pcp_predicates
function traverse_predicate(child, div_id, metrics) {
    // XXX
    metrics.push('kernel.all.load');
    // return 'function (metrics) { }';
    return 'function (metrics) { $("#' + div_id + '").html(JSON.stringify(metrics)); }';
}



return function (callback) {
    // install the predicate-related callback functions & config parameters
    top.rhck_predicate_fetch_every = 5000; // milliseconds
    top.rhck_predicate_pmwebapi = pmwebd + pmwebapi_url;
    top.rhck_predicate_metric_names = []; // will be filled in via emit_js_predicate() later on
    top.rhck_predicate_fetch = checklist_predicate_fetch;
    top.rhck_predicate_notifiers = [];

    $.ajax({
        method: 'GET',
        cache: false,
        url: pmwebd + "/checklist/checklist.json"
    }).done(function (result) {
        var nodes = result.nodes;
        var node = ARGS["node"];
        var dashboard;
        var node_map = [];
        var panel;
        var markdown = "";

        // create a table mapping node names to node objects
        for (var i = 0; i < nodes.length; i++) {
            node_map[nodes[i].name] = nodes[i];
            nodes[i].children = [];
        }

        // populate the children field of the nodes
        for (var i = 0; i < nodes.length; i++) {
            var child = nodes[i].name
            var parents = nodes[i].parents;
            for (var p = 0;
                (parents != undefined && p < parents.length); p++) {
                var pn = parents[p];
                // FIXME debug code
                console.log(pn, "(", node_map[pn].name, ") -> ", child, "(", node_map[child].name, ")");
                if (pn in node_map) {
                    node_map[pn].children.push(child);
                }
            }
        }

        // url parameter to identify checklist node
        if (node == undefined) {
            node = "Start";
        }

        // find
        if (node in node_map) {
            panel = node_map[node];
        } else {
            alert("checklist node " + node + "not found");
            callback(dashboard);
            return;
        }

        // Intialize a skeleton with nothing but a rows array and service object
        dashboard = {
            style: 'light',
            rows: [],
            services: {},
            refresh: '1s', // XXX: parametrize
            style: 'light',
            nav: [{
                type: "timepicker",
                collapse: false,
                enable: true,
                time_options: ["5m", "15m", "1h", "6h",
                    "12h", "24h", "2d", "7d",
                    "30d", "90d", "365d"
                ],
                refresh_intervals: ["0.1s", "0.25s", "0.5s", "1s", "2s", "5s", "10s", "30s", "1m"],
                now: true
            }],
            time: {
                from: "now-1m", // overriden with ARGS[from]
                to: "now" // overriden with ARGS[to]
            }
        };

        // Set a title
        dashboard.title = 'PCP CHECKLIST ' + node;

        // create navigation links/menus at the top

        // grafana etc. normally suppresses the link-following-reload action of markdown
        // or html links.  To overcome this, we emit a special <a href=javascript:....>
        // link instead.
        function emit_js_a_href(url, text) {
            return "<a href=\"javascript:window.location.replace('" + url + "'); window.location.reload(true);\">" + text + "</a>";
        }

        // register a <script></script><div></div> to be populated by javascript code
        function emit_js_onload_div(code, id) {
            return '<script>function onload_' + id + '() {' + code + '}</script><img src="data:image/gif;base64,R0lGODlhAQABAIAAAAAAAP///yH5BAEAAAAALAAAAAABAAEAAAIBRAA7" onload="onload_' + id + '()"><div id="' + id + '"></div>';
        }

        // create navigation links back up the graph
        var html = " | ";
        html += emit_js_a_href(pmwebd + checklist_url, "<b>RESTART</b>") + " | ";
        if ("parents" in panel) {
            for (var parent of panel.parents) {
                html += emit_js_a_href(pmwebd + checklist_url + "?node=" + encodeURIComponent(parent), parent);
                html += " | ";
            }
        }
        dashboard.rows.push({
            title: '',
            height: '0',
            panels: [{
                title: 'Navigation up',
                type: 'text',
                mode: 'html',
                span: 12,
                fill: 1,
                content: html
            }]
        });

        var html = " | ";
        if ("children" in panel) {
            $.each(panel.children, function (index, child) {
                var metrics = [];
                var l;
                var div_id = 'predicate_' + index + '_status'; // must also be a javascript identifier part
                for (var child of panel.children) {
                    l = traverse_predicate(child, div_id, metrics);
                }

                html += emit_js_onload_div('top.rhck_predicate_metric_names.push.apply(top.rhck_predicate_metric_names,' + JSON.stringify(metrics) + ');' +
                                           'top.rhck_predicate_notifiers.push(' + l + ');',
                                           div_id);

                html += emit_js_a_href(pmwebd + checklist_url + "?node=" + encodeURIComponent(child), child);
                html += " | ";
            });

            // start up periodic function to poll pcp metrics & call notifiers
            html += emit_js_onload_div('top.rhck_predicate_fetch();', 'predicate_fetch');
        }
        dashboard.rows.push({
            title: '',
            height: '0',
            panels: [{
                title: 'Possible Causes',
                type: 'text',
                mode: 'html',
                span: 12,
                fill: 1,
                content: html
            }]
        });

        // create description panel
        var markdown = "";
        if ("description" in panel) {
            markdown = panel.description;
        } else {
            markdown = "(no description)";
        }
        dashboard.rows.push({
            title: '',
            height: '0',
            panels: [{
                title: 'Description',
                type: 'text',
                span: 12,
                fill: 1,
                content: markdown
            }]
        });

        // create pcp metrics panel
        if ("pcp_metrics" in panel) {
            create_metric_panel(dashboard, panel);
        }

        // when dashboard is composed call the callback function and
        // pass the dashboard
        callback(dashboard);
    });
}
