/** @scratch /configuration/config.js/1
 * == Configuration
 * config.js is where you will find the core Grafana configuration. This file contains parameter that
 * must be set before Grafana is run for the first time.
 */
define(['settings'],
function (Settings) {
  return new Settings({
    graphiteUrl: "http://"+window.location.hostname+":"+window.location.port+"/graphite",
    timezoneOffset: "0000",
    unsaved_changes_warning: true,
    panel_names: [
      'text',
      'graphite'
    ]
  });
});
