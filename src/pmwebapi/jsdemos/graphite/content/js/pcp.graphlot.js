(function($) {
    $.QueryStringAny = function(q) {
        a = window.location.search.substr(1).split('&');
        if (a == "") return "";
        var b = "";
        for (var i = 0; i < a.length; ++i)
        {
            var p=a[i].split('=');
            if (p.length != 2) continue;
            if (q != p[0]) continue;
            return decodeURIComponent(p[1].replace(/\+/g, " "));
        }
        return "";
    };

    $.QueryStringAll = function(q) {
        a = window.location.search.substr(1).split('&');
        if (a == "") return "";
        var b = [];
        for (var i = 0; i < a.length; ++i)
        {
            var p=a[i].split('=');
            if (p.length != 2) continue;
            if (q != p[0]) continue;
            b.push (decodeURIComponent(p[1].replace(/\+/g, " ")));
        }
        return b;
    };

    // This is usually done by the graphite/graphlot/views.py (graphlot_render) function.
    $.graphlot_setup_from_parameters = function() {
        var targets = this.QueryStringAll("target");
        for (var i=0; i<targets.length; i++) {
            var target = targets[i];
            var new_row = $('<tr class="g_metricrow"><td><a href=#><span class="g_metricname">'+target+'</span></a></td><td><a href=#><span class="g_yaxis">one</span></a></td><td class="g_killrow"><img src="../content/img/delete.gif"></td></tr>');
            $('#g_wrap').find('.g_newmetricrow').before(new_row);
        }
        var y2targets = this.QueryStringAll("y2target");
        for (var i=0; i<y2targets.length; i++) {
            var target = y2targets[i];
            var new_row = $('<tr class="g_metricrow"><td><a href=#><span class="g_metricname">'+target+'</span></a></td><td><a href=#><span class="g_yaxis">two</span></a></td><td class="g_killrow"><img src="../content/img/delete.gif"></td></tr>');
            $('#g_wrap').find('.g_newmetricrow').before(new_row);
        }
        var from = this.QueryStringAny("from");
        if (from != "") {
            $('#g_wrap').find('.g_from').text(from);
        }
        var until = this.QueryStringAny("until");
        if (until != "") {
            $('#g_wrap').find('.g_until').text(from);
        }

    };
})(jQuery);
