// == Configuration
// config.js is where you will find the core Grafana configuration. This file contains parameter that
// must be set before Grafana is run for the first time.

define(['settings'], function(Settings) {
  return new Settings({
      datasources: {
        graphite: {
          type: 'graphite',
          url: window.location.protocol."//"+window.location.hostname+":"+window.location.port+"/graphite",
        },
      },
      
      // specify the limit for dashboard search results
      search: {
        max_results: 100
      },

      // default home dashboard
      default_route: '/dashboard/file/default.json',

      // set to false to disable unsaved changes warning
      unsaved_changes_warning: true,

      // set the default timespan for the playlist feature
      // Example: "1m", "1h"
      playlist_timespan: "1m",

      // Change window title prefix from 'Grafana - <dashboard title>'
      window_title_prefix: 'PCP/Grafana - ',
    });
});



