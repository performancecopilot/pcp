/*
 * Copyright (c) 2014 DataTorrent, Inc. ALL Rights Reserved.
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *   http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */
'use strict';

angular.module('ui.dashboard', ['ui.bootstrap', 'ui.sortable']);

angular.module('ui.dashboard')
  .directive('dashboard', ['WidgetModel', 'WidgetDefCollection', '$modal', 'DashboardState', '$log', function (WidgetModel, WidgetDefCollection, $modal, DashboardState, $log) {
    return {
      restrict: 'A',
      templateUrl: function(element, attr) {
        return attr.templateUrl ? attr.templateUrl : 'template/dashboard.html';
      },
      scope: true,

      controller: ['$scope', '$attrs', function (scope, attrs) {
        // default options
        var defaults = {
          stringifyStorage: true,
          hideWidgetSettings: false,
          hideWidgetClose: false,
          settingsModalOptions: {
            templateUrl: 'template/widget-settings-template.html',
            controller: 'WidgetSettingsCtrl'
          },
          onSettingsClose: function(result, widget) { // NOTE: dashboard scope is also passed as 3rd argument
            jQuery.extend(true, widget, result);
          },
          onSettingsDismiss: function(reason) { // NOTE: dashboard scope is also passed as 2nd argument
            $log.info('widget settings were dismissed. Reason: ', reason);
          }
        };

        // from dashboard="options"
        // scope.options = scope.$eval(attrs.dashboard);

        // extend default settingsModalOptions
        // scope.options.settingsModalOptions = scope.options.settingsModalOptions || {};

        // extend options with defaults
        // angular.extend(defaults.settingsModalOptions, scope.options.settingsModalOptions);
        // angular.extend(scope.options.settingsModalOptions, defaults.settingsModalOptions);
        // angular.extend(defaults, scope.options);
        // angular.extend(scope.options, defaults);

        // from dashboard="options"
        scope.options = scope.$eval(attrs.dashboard);

        // Deep options
        scope.options.settingsModalOptions = scope.options.settingsModalOptions || {};
        _.each(['settingsModalOptions'], function(key) {
          // Ensure it exists on scope.options
          scope.options[key] = scope.options[key] || {};
          // Set defaults
          _.defaults(scope.options[key], defaults[key]);
        });

        // Shallow options
        _.defaults(scope.options, defaults);

        // jQuery.extend(true, defaults, scope.options);
        // jQuery.extend(scope.options, defaults);

        var sortableDefaults = {
          stop: function () {
            scope.saveDashboard();
          },
          handle: '.widget-header'
        };
        scope.sortableOptions = angular.extend({}, sortableDefaults, scope.options.sortableOptions || {});

      }],
      link: function (scope) {

        // Save default widget config for reset
        scope.defaultWidgets = scope.options.defaultWidgets;

        //scope.widgetDefs = scope.options.widgetDefinitions;
        scope.widgetDefs = new WidgetDefCollection(scope.options.widgetDefinitions);
        var count = 1;

        // Instantiate new instance of dashboard state
        scope.dashboardState = new DashboardState(
          scope.options.storage,
          scope.options.storageId,
          scope.options.storageHash,
          scope.widgetDefs,
          scope.options.stringifyStorage
        );

        /**
         * Instantiates a new widget on the dashboard
         * @param {Object} widgetToInstantiate The definition object of the widget to be instantiated
         */
        scope.addWidget = function (widgetToInstantiate, doNotSave) {
          var defaultWidgetDefinition = scope.widgetDefs.getByName(widgetToInstantiate.name);
          if (!defaultWidgetDefinition) {
            throw 'Widget ' + widgetToInstantiate.name + ' is not found.';
          }

          // Determine the title for the new widget
          var title;
          if (widgetToInstantiate.title) {
            title = widgetToInstantiate.title;
          } else if (defaultWidgetDefinition.title) {
            title = defaultWidgetDefinition.title;
          } else {
            title = 'Widget ' + count++;
          }

          // Deep extend a new object for instantiation
          widgetToInstantiate = jQuery.extend(true, {}, defaultWidgetDefinition, widgetToInstantiate);

          // Instantiation
          var widget = new WidgetModel(widgetToInstantiate, {
            title: title
          });

          scope.widgets.push(widget);
          if (!doNotSave) {
            scope.saveDashboard();
          }

          return widget;
        };

        /**
         * Removes a widget instance from the dashboard
         * @param  {Object} widget The widget instance object (not a definition object)
         */
        scope.removeWidget = function (widget) {
          scope.widgets.splice(_.indexOf(scope.widgets, widget), 1);
          scope.saveDashboard();
        };

        /**
         * Opens a dialog for setting and changing widget properties
         * @param  {Object} widget The widget instance object
         */
        scope.openWidgetSettings = function (widget) {

          // Set up $modal options 
          var options = _.defaults(
            { scope: scope },
            widget.settingsModalOptions,
            scope.options.settingsModalOptions);

          // Ensure widget is resolved
          options.resolve = {
            widget: function () {
              return widget;
            }
          };
          
          // Create the modal
          var modalInstance = $modal.open(options);
          var onClose = widget.onSettingsClose || scope.options.onSettingsClose;
          var onDismiss = widget.onSettingsDismiss || scope.options.onSettingsDismiss;

          // Set resolve and reject callbacks for the result promise
          modalInstance.result.then(
            function (result) {

              // Call the close callback
              onClose(result, widget, scope);

              //AW Persist title change from options editor
              scope.$emit('widgetChanged', widget);
            },
            function (reason) {
              
              // Call the dismiss callback
              onDismiss(reason, scope);

            }
          );

        };

        /**
         * Remove all widget instances from dashboard
         */
        scope.clear = function (doNotSave) {
          scope.widgets = [];
          if (doNotSave === true) {
            return;
          }
          scope.saveDashboard();
        };

        /**
         * Used for preventing default on click event
         * @param {Object} event     A click event
         * @param {Object} widgetDef A widget definition object
         */
        scope.addWidgetInternal = function (event, widgetDef) {
          event.preventDefault();
          scope.addWidget(widgetDef);
        };

        /**
         * Uses dashboardState service to save state
         */
        scope.saveDashboard = function (force) {
          if (!scope.options.explicitSave) {
            scope.dashboardState.save(scope.widgets);
          } else {
            if (!angular.isNumber(scope.options.unsavedChangeCount)) {
              scope.options.unsavedChangeCount = 0;
            }
            if (force) {
              scope.options.unsavedChangeCount = 0;
              scope.dashboardState.save(scope.widgets);

            } else {
              ++scope.options.unsavedChangeCount;
            }
          }
        };

        /**
         * Wraps saveDashboard for external use.
         */
        scope.externalSaveDashboard = function() {
          scope.saveDashboard(true);
        };

        /**
         * Clears current dash and instantiates widget definitions
         * @param  {Array} widgets Array of definition objects
         */
        scope.loadWidgets = function (widgets) {
          // AW dashboards are continuously saved today (no "save" button).
          //scope.defaultWidgets = widgets;
          scope.savedWidgetDefs = widgets;
          scope.clear(true);
          _.each(widgets, function (widgetDef) {
            scope.addWidget(widgetDef, true);
          });
        };

        /**
         * Resets widget instances to default config
         * @return {[type]} [description]
         */
        scope.resetWidgetsToDefault = function () {
          scope.loadWidgets(scope.defaultWidgets);
          scope.saveDashboard();
        };

        // Set default widgets array
        var savedWidgetDefs = scope.dashboardState.load();

        // Success handler
        function handleStateLoad(saved) {
          scope.options.unsavedChangeCount = 0;
          if (saved && saved.length) {
            scope.loadWidgets(saved);
          } else if (scope.defaultWidgets) {
            scope.loadWidgets(scope.defaultWidgets);
          } else {
            scope.clear(true);
          }
        }

        if (angular.isArray(savedWidgetDefs)) {
          handleStateLoad(savedWidgetDefs);
        } else if (savedWidgetDefs && angular.isObject(savedWidgetDefs) && angular.isFunction(savedWidgetDefs.then)) {
          savedWidgetDefs.then(handleStateLoad, handleStateLoad);
        } else {
          handleStateLoad();
        }

        // expose functionality externally
        // functions are appended to the provided dashboard options
        scope.options.addWidget = scope.addWidget;
        scope.options.loadWidgets = scope.loadWidgets;
        scope.options.saveDashboard = scope.externalSaveDashboard;
        scope.options.removeWidget = scope.removeWidget;
        scope.options.openWidgetSettings = scope.openWidgetSettings;

        // save state
        scope.$on('widgetChanged', function (event) {
          event.stopPropagation();
          scope.saveDashboard();
        });
      }
    };
  }]);
