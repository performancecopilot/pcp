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

angular.module('ui.dashboard')
  .directive('dashboardLayouts', ['LayoutStorage', '$timeout', '$modal',
    function(LayoutStorage, $timeout, $modal) {
      return {
        scope: true,
        templateUrl: function(element, attr) {
          return attr.templateUrl ? attr.templateUrl : 'template/dashboard-layouts.html';
        },
        link: function(scope, element, attrs) {

          scope.options = scope.$eval(attrs.dashboardLayouts);

          var layoutStorage = new LayoutStorage(scope.options);

          scope.layouts = layoutStorage.layouts;

          scope.createNewLayout = function() {
            var newLayout = {
              title: 'Custom',
              defaultWidgets: scope.options.defaultWidgets || []
            };
            layoutStorage.add(newLayout);
            scope.makeLayoutActive(newLayout);
            layoutStorage.save();
            return newLayout;
          };

          scope.removeLayout = function(layout) {
            layoutStorage.remove(layout);
            layoutStorage.save();
          };

          scope.makeLayoutActive = function(layout) {

            var current = layoutStorage.getActiveLayout();

            if (current && current.dashboard.unsavedChangeCount) {
              var modalInstance = $modal.open({
                templateUrl: 'template/save-changes-modal.html',
                resolve: {
                  layout: function() {
                    return layout;
                  }
                },
                controller: 'SaveChangesModalCtrl'
              });

              // Set resolve and reject callbacks for the result promise
              modalInstance.result.then(
                function() {
                  current.dashboard.saveDashboard();
                  scope._makeLayoutActive(layout);
                },
                function() {
                  scope._makeLayoutActive(layout);
                }
              );
            } else {
              scope._makeLayoutActive(layout);
            }

          };

          scope._makeLayoutActive = function(layout) {
            angular.forEach(scope.layouts, function(l) {
              if (l !== layout) {
                l.active = false;
              } else {
                l.active = true;
              }
            });
            layoutStorage.save();
          };

          scope.isActive = function(layout) {
            return !!layout.active;
          };

          scope.editTitle = function(layout) {
            if (layout.locked) {
              return;
            }

            var input = element.find('input[data-layout="' + layout.id + '"]');
            layout.editingTitle = true;

            $timeout(function() {
              input.focus()[0].setSelectionRange(0, 9999);
            });
          };

          // saves whatever is in the title input as the new title
          scope.saveTitleEdit = function(layout) {
            layout.editingTitle = false;
            layoutStorage.save();
          };

          scope.options.saveLayouts = function() {
            layoutStorage.save(true);
          };
          scope.options.addWidget = function() {
            var layout = layoutStorage.getActiveLayout();
            if (layout) {
              layout.dashboard.addWidget.apply(layout.dashboard, arguments);
            }
          };
          scope.options.loadWidgets = function() {
            var layout = layoutStorage.getActiveLayout();
            if (layout) {
              layout.dashboard.loadWidgets.apply(layout.dashboard, arguments);
            }
          };
          scope.options.saveDashboard = function() {
            var layout = layoutStorage.getActiveLayout();
            if (layout) {
              layout.dashboard.saveDashboard.apply(layout.dashboard, arguments);
            }
          };

          var sortableDefaults = {
            stop: function() {
              scope.options.saveLayouts();
            },
          };
          scope.sortableOptions = angular.extend({}, sortableDefaults, scope.options.sortableOptions || {});
        }
      };
    }
  ]);
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

angular.module('ui.dashboard')
  .directive('widget', ['$injector', function ($injector) {

    return {

      controller: 'DashboardWidgetCtrl',

      link: function (scope) {

        var widget = scope.widget;
        var dataModelType = widget.dataModelType;

        // set up data source
        if (dataModelType) {
          var DataModelConstructor; // data model constructor function

          if (angular.isFunction(dataModelType)) {
            DataModelConstructor = dataModelType;
          } else if (angular.isString(dataModelType)) {
            $injector.invoke([dataModelType, function (DataModelType) {
              DataModelConstructor = DataModelType;
            }]);
          } else {
            throw new Error('widget dataModelType should be function or string');
          }

          var ds;
          if (widget.dataModelArgs) {
            ds = new DataModelConstructor(widget.dataModelArgs);
          } else {
            ds = new DataModelConstructor();
          }
          widget.dataModel = ds;
          ds.setup(widget, scope);
          ds.init();
          scope.$on('$destroy', _.bind(ds.destroy,ds));
        }

        // Compile the widget template, emit add event
        scope.compileTemplate();
        scope.$emit('widgetAdded', widget);

      }

    };
  }]);

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

angular.module('ui.dashboard')
  .factory('LayoutStorage', function() {

    var noopStorage = {
      setItem: function() {

      },
      getItem: function() {

      },
      removeItem: function() {

      }
    };

    

    function LayoutStorage(options) {

      var defaults = {
        storage: noopStorage,
        storageHash: '',
        stringifyStorage: true
      };

      angular.extend(defaults, options);
      angular.extend(options, defaults);

      this.id = options.storageId;
      this.storage = options.storage;
      this.storageHash = options.storageHash;
      this.stringifyStorage = options.stringifyStorage;
      this.widgetDefinitions = options.widgetDefinitions;
      this.defaultLayouts = options.defaultLayouts;
      this.lockDefaultLayouts = options.lockDefaultLayouts;
      this.widgetButtons = options.widgetButtons;
      this.explicitSave = options.explicitSave;
      this.defaultWidgets = options.defaultWidgets;
      this.settingsModalOptions = options.settingsModalOptions;
      this.onSettingsClose = options.onSettingsClose;
      this.onSettingsDismiss = options.onSettingsDismiss;
      this.options = options;
      this.options.unsavedChangeCount = 0;

      this.layouts = [];
      this.states = {};
      this.load();
      this._ensureActiveLayout();
    }

    LayoutStorage.prototype = {

      add: function(layouts) {
        if (!angular.isArray(layouts)) {
          layouts = [layouts];
        }
        var self = this;
        angular.forEach(layouts, function(layout) {
          layout.dashboard = layout.dashboard || {};
          layout.dashboard.storage = self;
          layout.dashboard.storageId = layout.id = self._getLayoutId.call(self,layout);
          layout.dashboard.widgetDefinitions = self.widgetDefinitions;
          layout.dashboard.stringifyStorage = false;
          layout.dashboard.defaultWidgets = layout.defaultWidgets || self.defaultWidgets;
          layout.dashboard.widgetButtons = self.widgetButtons;
          layout.dashboard.explicitSave = self.explicitSave;
          layout.dashboard.settingsModalOptions = self.settingsModalOptions;
          layout.dashboard.onSettingsClose = self.onSettingsClose;
          layout.dashboard.onSettingsDismiss = self.onSettingsDismiss;
          self.layouts.push(layout);
        });
      },

      remove: function(layout) {
        var index = this.layouts.indexOf(layout);
        if (index >= 0) {
          this.layouts.splice(index, 1);
          delete this.states[layout.id];

          // check for active
          if (layout.active && this.layouts.length) {
            var nextActive = index > 0 ? index - 1 : 0;
            this.layouts[nextActive].active = true;
          }
        }
      },

      save: function() {

        var state = {
          layouts: this._serializeLayouts(),
          states: this.states,
          storageHash: this.storageHash
        };

        if (this.stringifyStorage) {
          state = JSON.stringify(state);
        }

        this.storage.setItem(this.id, state);
        this.options.unsavedChangeCount = 0;
      },

      load: function() {

        var serialized = this.storage.getItem(this.id);

        this.clear();

        if (serialized) {
          // check for promise
          if (angular.isObject(serialized) && angular.isFunction(serialized.then)) {
            this._handleAsyncLoad(serialized);
          } else {
            this._handleSyncLoad(serialized);
          }
        } else {
          this._addDefaultLayouts();
        }
      },

      clear: function() {
        this.layouts = [];
        this.states = {};
      },

      setItem: function(id, value) {
        this.states[id] = value;
        this.save();
      },

      getItem: function(id) {
        return this.states[id];
      },

      removeItem: function(id) {
        delete this.states[id];
        this.save();
      },

      getActiveLayout: function() {
        var len = this.layouts.length;
        for (var i = 0; i < len; i++) {
          var layout = this.layouts[i];
          if (layout.active) {
            return layout;
          }
        }
        return false;
      },

      _addDefaultLayouts: function() {
        var self = this;
        var defaults = this.lockDefaultLayouts ? { locked: true } : {};
        angular.forEach(this.defaultLayouts, function(layout) {
          self.add(angular.extend(_.clone(defaults), layout));
        });
      },

      _serializeLayouts: function() {
        var result = [];
        angular.forEach(this.layouts, function(l) {
          result.push({
            title: l.title,
            id: l.id,
            active: l.active,
            locked: l.locked,
            defaultWidgets: l.dashboard.defaultWidgets
          });
        });
        return result;
      },

      _handleSyncLoad: function(serialized) {
        
        var deserialized;

        if (this.stringifyStorage) {
          try {

            deserialized = JSON.parse(serialized);

          } catch (e) {
            this._addDefaultLayouts();
            return;
          }
        } else {

          deserialized = serialized;

        }

        if (this.storageHash !== deserialized.storageHash) {
          this._addDefaultLayouts();
          return;
        }
        this.states = deserialized.states;
        this.add(deserialized.layouts);
      },

      _handleAsyncLoad: function(promise) {
        var self = this;
        promise.then(
          angular.bind(self, this._handleSyncLoad),
          angular.bind(self, this._addDefaultLayouts)
        );
      },

      _ensureActiveLayout: function() {
        for (var i = 0; i < this.layouts.length; i++) {
          var layout = this.layouts[i];
          if (layout.active) {
            return;
          }
        }
        if (this.layouts[0]) {
          this.layouts[0].active = true;
        }
      },

      _getLayoutId: function(layout) {
        if (layout.id) {
          return layout.id;
        }
        var max = 0;
        for (var i = 0; i < this.layouts.length; i++) {
          var id = this.layouts[i].id;
          max = Math.max(max, id * 1);
        }
        return max + 1;
      }

    };
    return LayoutStorage;
  });
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

angular.module('ui.dashboard')
  .factory('DashboardState', ['$log', '$q', function ($log, $q) {
    function DashboardState(storage, id, hash, widgetDefinitions, stringify) {
      this.storage = storage;
      this.id = id;
      this.hash = hash;
      this.widgetDefinitions = widgetDefinitions;
      this.stringify = stringify;
    }

    DashboardState.prototype = {
      /**
       * Takes array of widget instance objects, serializes, 
       * and saves state.
       * 
       * @param  {Array} widgets  scope.widgets from dashboard directive
       * @return {Boolean}        true on success, false on failure
       */
      save: function (widgets) {
        
        if (!this.storage) {
          return true;
        }

        var serialized = _.map(widgets, function (widget) {
          var widgetObject = {
            title: widget.title,
            name: widget.name,
            style: widget.style,
            size: widget.size,
            dataModelOptions: widget.dataModelOptions,
            storageHash: widget.storageHash,
            attrs: widget.attrs
          };

          return widgetObject;
        });

        var item = { widgets: serialized, hash: this.hash };

        if (this.stringify) {
          item = JSON.stringify(item);
        }

        this.storage.setItem(this.id, item);
        return true;
      },

      /**
       * Loads dashboard state from the storage object.
       * Can handle a synchronous response or a promise.
       * 
       * @return {Array|Promise} Array of widget definitions or a promise
       */
      load: function () {

        if (!this.storage) {
          return null;
        }

        var serialized;

        // try loading storage item
        serialized = this.storage.getItem( this.id );

        if (serialized) {
          // check for promise
          if (angular.isObject(serialized) && angular.isFunction(serialized.then)) {
            return this._handleAsyncLoad(serialized);
          }
          // otherwise handle synchronous load
          return this._handleSyncLoad(serialized);
        } else {
          return null;
        }
      },

      _handleSyncLoad: function(serialized) {

        var deserialized, result = [];

        if (!serialized) {
          return null;
        }

        if (this.stringify) {
          try { // to deserialize the string

            deserialized = JSON.parse(serialized);

          } catch (e) {

            // bad JSON, log a warning and return
            $log.warn('Serialized dashboard state was malformed and could not be parsed: ', serialized);
            return null;

          }
        }
        else {
          deserialized = serialized;
        }

        // check hash against current hash
        if (deserialized.hash !== this.hash) {

          $log.info('Serialized dashboard from storage was stale (old hash: ' + deserialized.hash + ', new hash: ' + this.hash + ')');
          this.storage.removeItem(this.id);
          return null;

        }

        // Cache widgets
        var savedWidgetDefs = deserialized.widgets;

        // instantiate widgets from stored data
        for (var i = 0; i < savedWidgetDefs.length; i++) {

          // deserialized object
          var savedWidgetDef = savedWidgetDefs[i];

          // widget definition to use
          var widgetDefinition = this.widgetDefinitions.getByName(savedWidgetDef.name);

          // check for no widget
          if (!widgetDefinition) {
            // no widget definition found, remove and return false
            $log.warn('Widget with name "' + savedWidgetDef.name + '" was not found in given widget definition objects');
            continue;
          }

          // check widget-specific storageHash
          if (widgetDefinition.hasOwnProperty('storageHash') && widgetDefinition.storageHash !== savedWidgetDef.storageHash) {
            // widget definition was found, but storageHash was stale, removing storage
            $log.info('Widget Definition Object with name "' + savedWidgetDef.name + '" was found ' +
              'but the storageHash property on the widget definition is different from that on the ' +
              'serialized widget loaded from storage. hash from storage: "' + savedWidgetDef.storageHash + '"' +
              ', hash from WDO: "' + widgetDefinition.storageHash + '"');
            continue;
          }

          // push instantiated widget to result array
          result.push(savedWidgetDef);
        }

        return result;
      },

      _handleAsyncLoad: function(promise) {
        var self = this;
        var deferred = $q.defer();
        promise.then(
          // success
          function(res) {
            var result = self._handleSyncLoad(res);
            if (result) {
              deferred.resolve(result);
            } else {
              deferred.reject(result);
            }
          },
          // failure
          function(res) {
            deferred.reject(res);
          }
        );

        return deferred.promise;
      }

    };
    return DashboardState;
  }]);
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

angular.module('ui.dashboard')
  .factory('WidgetDataModel', function () {
    function WidgetDataModel() {
    }

    WidgetDataModel.prototype = {
      setup: function (widget, scope) {
        this.dataAttrName = widget.dataAttrName;
        this.dataModelOptions = widget.dataModelOptions;
        this.widgetScope = scope;
      },

      updateScope: function (data) {
        this.widgetScope.widgetData = data;
      },

      init: function () {
        // to be overridden by subclasses
      },

      destroy: function () {
        // to be overridden by subclasses
      }
    };

    return WidgetDataModel;
  });
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

angular.module('ui.dashboard')
  .factory('WidgetDefCollection', function () {
    function WidgetDefCollection(widgetDefs) {
      this.push.apply(this, widgetDefs);

      // build (name -> widget definition) map for widget lookup by name
      var map = {};
      _.each(widgetDefs, function (widgetDef) {
        map[widgetDef.name] = widgetDef;
      });
      this.map = map;
    }

    WidgetDefCollection.prototype = Object.create(Array.prototype);

    WidgetDefCollection.prototype.getByName = function (name) {
      return this.map[name];
    };

    return WidgetDefCollection;
  });
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

angular.module('ui.dashboard')
  .factory('WidgetModel', function ($log) {
    // constructor for widget model instances
    function WidgetModel(Class, overrides) {
      var defaults = {
          title: 'Widget',
          name: Class.name,
          attrs: Class.attrs,
          dataAttrName: Class.dataAttrName,
          dataModelType: Class.dataModelType,
          dataModelArgs: Class.dataModelArgs, // used in data model constructor, not serialized
          //AW Need deep copy of options to support widget options editing
          dataModelOptions: Class.dataModelOptions,
          settingsModalOptions: Class.settingsModalOptions,
          onSettingsClose: Class.onSettingsClose,
          onSettingsDismiss: Class.onSettingsDismiss,
          style: Class.style || {},
          size: Class.size || {},
          enableVerticalResize: (Class.enableVerticalResize === false) ? false : true
        };

      overrides = overrides || {};
      angular.extend(this, angular.copy(defaults), overrides);
      this.containerStyle = { width: '33%' }; // default width
      this.contentStyle = {};
      this.updateContainerStyle(this.style);

      if (Class.templateUrl) {
        this.templateUrl = Class.templateUrl;
      } else if (Class.template) {
        this.template = Class.template;
      } else {
        var directive = Class.directive || Class.name;
        this.directive = directive;
      }

      if (this.size && _.has(this.size, 'height')) {
        this.setHeight(this.size.height);
      }

      if (this.style && _.has(this.style, 'width')) { //TODO deprecate style attribute
        this.setWidth(this.style.width);
      }

      if (this.size && _.has(this.size, 'width')) {
        this.setWidth(this.size.width);
      }
    }

    WidgetModel.prototype = {
      // sets the width (and widthUnits)
      setWidth: function (width, units) {
        width = width.toString();
        units = units || width.replace(/^[-\.\d]+/, '') || '%';

        this.widthUnits = units;
        width = parseFloat(width);

        if (width < 0 || isNaN(width)) {
          $log.warn('malhar-angular-dashboard: setWidth was called when width was ' + width);
          return false;
        }

        if (units === '%') {
          width = Math.min(100, width);
          width = Math.max(0, width);
        }

        this.containerStyle.width = width + '' + units;

        this.updateSize(this.containerStyle);

        return true;
      },

      setHeight: function (height) {
        this.contentStyle.height = height;
        this.updateSize(this.contentStyle);
      },

      setStyle: function (style) {
        this.style = style;
        this.updateContainerStyle(style);
      },

      updateSize: function (size) {
        angular.extend(this.size, size);
      },

      updateContainerStyle: function (style) {
        angular.extend(this.containerStyle, style);
      }
    };

    return WidgetModel;
  });
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

angular.module('ui.dashboard')
  .controller('SaveChangesModalCtrl', ['$scope', '$modalInstance', 'layout', function ($scope, $modalInstance, layout) {
    
    // add layout to scope
    $scope.layout = layout;

    $scope.ok = function () {
      $modalInstance.close();
    };

    $scope.cancel = function () {
      $modalInstance.dismiss();
    };
  }]);
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

angular.module('ui.dashboard')
  .controller('DashboardWidgetCtrl', ['$scope', '$element', '$compile', '$window', '$timeout',
    function($scope, $element, $compile, $window, $timeout) {

      $scope.status = {
        isopen: false
      };

      // Fills "container" with compiled view
      $scope.makeTemplateString = function() {

        var widget = $scope.widget;

        // First, build template string
        var templateString = '';

        if (widget.templateUrl) {

          // Use ng-include for templateUrl
          templateString = '<div ng-include="\'' + widget.templateUrl + '\'"></div>';

        } else if (widget.template) {

          // Direct string template
          templateString = widget.template;

        } else {

          // Assume attribute directive
          templateString = '<div ' + widget.directive;

          // Check if data attribute was specified
          if (widget.dataAttrName) {
            widget.attrs = widget.attrs || {};
            widget.attrs[widget.dataAttrName] = 'widgetData';
          }

          // Check for specified attributes
          if (widget.attrs) {

            // First check directive name attr
            if (widget.attrs[widget.directive]) {
              templateString += '="' + widget.attrs[widget.directive] + '"';
            }

            // Add attributes
            _.each(widget.attrs, function(value, attr) {

              // make sure we aren't reusing directive attr
              if (attr !== widget.directive) {
                templateString += ' ' + attr + '="' + value + '"';
              }

            });
          }
          templateString += '></div>';
        }
        return templateString;
      };

      $scope.grabResizer = function(e) {

        var widget = $scope.widget;
        var widgetElm = $element.find('.widget');

        // ignore middle- and right-click
        if (e.which !== 1) {
          return;
        }

        e.stopPropagation();
        e.originalEvent.preventDefault();

        // get the starting horizontal position
        var initX = e.clientX;
        // console.log('initX', initX);

        // Get the current width of the widget and dashboard
        var pixelWidth = widgetElm.width();
        var pixelHeight = widgetElm.height();
        var widgetStyleWidth = widget.containerStyle.width;
        var widthUnits = widget.widthUnits;
        var unitWidth = parseFloat(widgetStyleWidth);

        // create marquee element for resize action
        var $marquee = angular.element('<div class="widget-resizer-marquee" style="height: ' + pixelHeight + 'px; width: ' + pixelWidth + 'px;"></div>');
        widgetElm.append($marquee);

        // determine the unit/pixel ratio
        var transformMultiplier = unitWidth / pixelWidth;

        // updates marquee with preview of new width
        var mousemove = function(e) {
          var curX = e.clientX;
          var pixelChange = curX - initX;
          var newWidth = pixelWidth + pixelChange;
          $marquee.css('width', newWidth + 'px');
        };

        // sets new widget width on mouseup
        var mouseup = function(e) {
          // remove listener and marquee
          jQuery($window).off('mousemove', mousemove);
          $marquee.remove();

          // calculate change in units
          var curX = e.clientX;
          var pixelChange = curX - initX;
          var unitChange = Math.round(pixelChange * transformMultiplier * 100) / 100;

          // add to initial unit width
          var newWidth = unitWidth * 1 + unitChange;
          widget.setWidth(newWidth, widthUnits);
          $scope.$emit('widgetChanged', widget);
          $scope.$apply();
          $scope.$broadcast('widgetResized', {
            width: newWidth
          });
        };

        jQuery($window).on('mousemove', mousemove).one('mouseup', mouseup);
      };

      //TODO refactor
      $scope.grabSouthResizer = function(e) {
        var widgetElm = $element.find('.widget');

        // ignore middle- and right-click
        if (e.which !== 1) {
          return;
        }

        e.stopPropagation();
        e.originalEvent.preventDefault();

        // get the starting horizontal position
        var initY = e.clientY;
        // console.log('initX', initX);

        // Get the current width of the widget and dashboard
        var pixelWidth = widgetElm.width();
        var pixelHeight = widgetElm.height();

        // create marquee element for resize action
        var $marquee = angular.element('<div class="widget-resizer-marquee" style="height: ' + pixelHeight + 'px; width: ' + pixelWidth + 'px;"></div>');
        widgetElm.append($marquee);

        // updates marquee with preview of new height
        var mousemove = function(e) {
          var curY = e.clientY;
          var pixelChange = curY - initY;
          var newHeight = pixelHeight + pixelChange;
          $marquee.css('height', newHeight + 'px');
        };

        // sets new widget width on mouseup
        var mouseup = function(e) {
          // remove listener and marquee
          jQuery($window).off('mousemove', mousemove);
          $marquee.remove();

          // calculate height change
          var curY = e.clientY;
          var pixelChange = curY - initY;

          //var widgetContainer = widgetElm.parent(); // widget container responsible for holding widget width and height
          var widgetContainer = widgetElm.find('.widget-content');

          var diff = pixelChange;
          var height = parseInt(widgetContainer.css('height'), 10);
          var newHeight = (height + diff);

          //$scope.widget.style.height = newHeight + 'px';

          $scope.widget.setHeight(newHeight + 'px');

          $scope.$emit('widgetChanged', $scope.widget);
          $scope.$apply(); // make AngularJS to apply style changes

          $scope.$broadcast('widgetResized', {
            height: newHeight
          });
        };

        jQuery($window).on('mousemove', mousemove).one('mouseup', mouseup);
      };

      // replaces widget title with input
      $scope.editTitle = function(widget) {
        var widgetElm = $element.find('.widget');
        widget.editingTitle = true;
        // HACK: get the input to focus after being displayed.
        $timeout(function() {
          widgetElm.find('form.widget-title input:eq(0)').focus()[0].setSelectionRange(0, 9999);
        });
      };

      // saves whatever is in the title input as the new title
      $scope.saveTitleEdit = function(widget) {
        widget.editingTitle = false;
        $scope.$emit('widgetChanged', widget);
      };

      $scope.compileTemplate = function() {
        var container = $scope.findWidgetContainer($element);
        var templateString = $scope.makeTemplateString();
        var widgetElement = angular.element(templateString);

        container.empty();
        container.append(widgetElement);
        $compile(widgetElement)($scope);
      };

      $scope.findWidgetContainer = function(element) {
        // widget placeholder is the first (and only) child of .widget-content
        return element.find('.widget-content');
      };
    }
  ]);
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

angular.module('ui.dashboard')
  .controller('WidgetSettingsCtrl', ['$scope', '$modalInstance', 'widget', function ($scope, $modalInstance, widget) {
    // add widget to scope
    $scope.widget = widget;

    // set up result object
    $scope.result = jQuery.extend(true, {}, widget);

    $scope.ok = function () {
      $modalInstance.close($scope.result);
    };

    $scope.cancel = function () {
      $modalInstance.dismiss('cancel');
    };
  }]);
angular.module("ui.dashboard").run(["$templateCache", function($templateCache) {

  $templateCache.put("template/alt-dashboard.html",
    "<div>\n" +
    "    <div class=\"btn-toolbar\" ng-if=\"!options.hideToolbar\">\n" +
    "        <div class=\"btn-group\" ng-if=\"!options.widgetButtons\">\n" +
    "            <span class=\"dropdown\" on-toggle=\"toggled(open)\">\n" +
    "              <button type=\"button\" class=\"btn btn-primary dropdown-toggle\" ng-disabled=\"disabled\">\n" +
    "                Button dropdown <span class=\"caret\"></span>\n" +
    "              </button>\n" +
    "              <ul class=\"dropdown-menu\" role=\"menu\">\n" +
    "                <li ng-repeat=\"widget in widgetDefs\">\n" +
    "                  <a href=\"#\" ng-click=\"addWidgetInternal($event, widget);\" class=\"dropdown-toggle\">{{widget.name}}</a>\n" +
    "                </li>\n" +
    "              </ul>\n" +
    "            </span>\n" +
    "        </div>\n" +
    "\n" +
    "        <div class=\"btn-group\" ng-if=\"options.widgetButtons\">\n" +
    "            <button ng-repeat=\"widget in widgetDefs\"\n" +
    "                    ng-click=\"addWidgetInternal($event, widget);\" type=\"button\" class=\"btn btn-primary\">\n" +
    "                {{widget.name}}\n" +
    "            </button>\n" +
    "        </div>\n" +
    "\n" +
    "        <button class=\"btn btn-warning\" ng-click=\"resetWidgetsToDefault()\">Default Widgets</button>\n" +
    "\n" +
    "        <button ng-if=\"options.storage && options.explicitSave\" ng-click=\"options.saveDashboard()\" class=\"btn btn-success\" ng-hide=\"!options.unsavedChangeCount\">{{ !options.unsavedChangeCount ? \"Alternative - No Changes\" : \"Save\" }}</button>\n" +
    "\n" +
    "        <button ng-click=\"clear();\" ng-hide=\"!widgets.length\" type=\"button\" class=\"btn btn-info\">Clear</button>\n" +
    "    </div>\n" +
    "\n" +
    "    <div ui-sortable=\"sortableOptions\" ng-model=\"widgets\" class=\"dashboard-widget-area\">\n" +
    "        <div ng-repeat=\"widget in widgets\" ng-style=\"widget.style\" class=\"widget-container\" widget>\n" +
    "            <div class=\"widget panel panel-default\">\n" +
    "                <div class=\"widget-header panel-heading\">\n" +
    "                    <h3 class=\"panel-title\">\n" +
    "                        <span class=\"widget-title\" ng-dblclick=\"editTitle(widget)\" ng-hide=\"widget.editingTitle\">{{widget.title}}</span>\n" +
    "                        <form action=\"\" class=\"widget-title\" ng-show=\"widget.editingTitle\" ng-submit=\"saveTitleEdit(widget)\">\n" +
    "                            <input type=\"text\" ng-model=\"widget.title\" class=\"form-control\">\n" +
    "                        </form>\n" +
    "                        <span class=\"label label-primary\" ng-if=\"!options.hideWidgetName\">{{widget.name}}</span>\n" +
    "                        <span ng-click=\"removeWidget(widget);\" class=\"glyphicon glyphicon-remove\" ng-if=\"!options.hideWidgetClose\"></span>\n" +
    "                        <span ng-click=\"openWidgetSettings(widget);\" class=\"glyphicon glyphicon-cog\" ng-if=\"!options.hideWidgetSettings\"></span>\n" +
    "                    </h3>\n" +
    "                </div>\n" +
    "                <div class=\"panel-body widget-content\"></div>\n" +
    "                <div class=\"widget-ew-resizer\" ng-mousedown=\"grabResizer($event)\"></div>\n" +
    "            </div>\n" +
    "        </div>\n" +
    "    </div>\n" +
    "</div>\n"
  );

  $templateCache.put("template/dashboard-layouts.html",
    "<ul ui-sortable=\"sortableOptions\" ng-model=\"layouts\" class=\"nav nav-tabs layout-tabs\">\n" +
    "    <li ng-repeat=\"layout in layouts\" ng-class=\"{ active: layout.active }\">\n" +
    "        <a ng-click=\"makeLayoutActive(layout)\">\n" +
    "            <span ng-dblclick=\"editTitle(layout)\" ng-show=\"!layout.editingTitle\">{{layout.title}}</span>\n" +
    "            <form action=\"\" class=\"layout-title\" ng-show=\"layout.editingTitle\" ng-submit=\"saveTitleEdit(layout)\">\n" +
    "                <input type=\"text\" ng-model=\"layout.title\" class=\"form-control\" data-layout=\"{{layout.id}}\">\n" +
    "            </form>\n" +
    "            <span ng-if=\"!layout.locked\" ng-click=\"removeLayout(layout)\" class=\"glyphicon glyphicon-remove remove-layout-icon\"></span>\n" +
    "            <!-- <span class=\"glyphicon glyphicon-pencil\"></span> -->\n" +
    "            <!-- <span class=\"glyphicon glyphicon-remove\"></span> -->\n" +
    "        </a>\n" +
    "    </li>\n" +
    "    <li>\n" +
    "        <a ng-click=\"createNewLayout()\">\n" +
    "            <span class=\"glyphicon glyphicon-plus\"></span>\n" +
    "        </a>\n" +
    "    </li>\n" +
    "</ul>\n" +
    "<div ng-repeat=\"layout in layouts | filter:isActive\" dashboard=\"layout.dashboard\" template-url=\"template/dashboard.html\"></div>"
  );

  $templateCache.put("template/dashboard.html",
    "<div>\n" +
    "    <div class=\"btn-toolbar\" ng-if=\"!options.hideToolbar\">\n" +
    "        <div class=\"btn-group\" ng-if=\"!options.widgetButtons\">\n" +
    "            <span class=\"dropdown\" on-toggle=\"toggled(open)\">\n" +
    "              <button type=\"button\" class=\"btn btn-primary dropdown-toggle\" data-toggle=\"dropdown\">\n" +
    "                Button dropdown <span class=\"caret\"></span>\n" +
    "              </button>\n" +
    "              <ul class=\"dropdown-menu\" role=\"menu\">\n" +
    "                <li ng-repeat=\"widget in widgetDefs\">\n" +
    "                  <a href=\"#\" ng-click=\"addWidgetInternal($event, widget);\" class=\"dropdown-toggle\"><span class=\"label label-primary\">{{widget.name}}</span></a>\n" +
    "                </li>\n" +
    "              </ul>\n" +
    "            </span>\n" +
    "    </div>\n" +
    "        <div class=\"btn-group\" ng-if=\"options.widgetButtons\">\n" +
    "            <button ng-repeat=\"widget in widgetDefs\"\n" +
    "                    ng-click=\"addWidgetInternal($event, widget);\" type=\"button\" class=\"btn btn-primary\">\n" +
    "                {{widget.name}}\n" +
    "            </button>\n" +
    "        </div>\n" +
    "\n" +
    "        <button class=\"btn btn-warning\" ng-click=\"resetWidgetsToDefault()\">Default Widgets</button>\n" +
    "\n" +
    "        <button ng-if=\"options.storage && options.explicitSave\" ng-click=\"options.saveDashboard()\" class=\"btn btn-success\" ng-disabled=\"!options.unsavedChangeCount\">{{ !options.unsavedChangeCount ? \"all saved\" : \"save changes (\" + options.unsavedChangeCount + \")\" }}</button>\n" +
    "\n" +
    "        <button ng-click=\"clear();\" type=\"button\" class=\"btn btn-info\">Clear</button>\n" +
    "    </div>\n" +
    "\n" +
    "    <div ui-sortable=\"sortableOptions\" ng-model=\"widgets\" class=\"dashboard-widget-area\">\n" +
    "        <div ng-repeat=\"widget in widgets\" ng-style=\"widget.containerStyle\" class=\"widget-container\" widget>\n" +
    "            <div class=\"widget panel panel-default\">\n" +
    "                <div class=\"widget-header panel-heading\">\n" +
    "                    <h3 class=\"panel-title\">\n" +
    "                        <span class=\"widget-title\" ng-dblclick=\"editTitle(widget)\" ng-hide=\"widget.editingTitle\">{{widget.title}}</span>\n" +
    "                        <form action=\"\" class=\"widget-title\" ng-show=\"widget.editingTitle\" ng-submit=\"saveTitleEdit(widget)\">\n" +
    "                            <input type=\"text\" ng-model=\"widget.title\" class=\"form-control\">\n" +
    "                        </form>\n" +
    "                        <span class=\"label label-primary\" ng-if=\"!options.hideWidgetName\">{{widget.name}}</span>\n" +
    "                        <span ng-click=\"removeWidget(widget);\" class=\"glyphicon glyphicon-remove\" ng-if=\"!options.hideWidgetClose\"></span>\n" +
    "                        <span ng-click=\"openWidgetSettings(widget);\" class=\"glyphicon glyphicon-cog\" ng-if=\"!options.hideWidgetSettings\"></span>\n" +
    "                    </h3>\n" +
    "                </div>\n" +
    "                <div class=\"panel-body widget-content\" ng-style=\"widget.contentStyle\"></div>\n" +
    "                <div class=\"widget-ew-resizer\" ng-mousedown=\"grabResizer($event)\"></div>\n" +
    "                <div ng-if=\"widget.enableVerticalResize\" class=\"widget-s-resizer\" ng-mousedown=\"grabSouthResizer($event)\"></div>\n" +
    "            </div>\n" +
    "        </div>\n" +
    "    </div>\n" +
    "</div>"
  );

  $templateCache.put("template/save-changes-modal.html",
    "<div class=\"modal-header\">\n" +
    "    <button type=\"button\" class=\"close\" data-dismiss=\"modal\" aria-hidden=\"true\" ng-click=\"cancel()\">&times;</button>\n" +
    "  <h3>Unsaved Changes to \"{{layout.title}}\"</h3>\n" +
    "</div>\n" +
    "\n" +
    "<div class=\"modal-body\">\n" +
    "    <p>You have {{layout.dashboard.unsavedChangeCount}} unsaved changes on this dashboard. Would you like to save them?</p>\n" +
    "</div>\n" +
    "\n" +
    "<div class=\"modal-footer\">\n" +
    "    <button type=\"button\" class=\"btn btn-default\" ng-click=\"cancel()\">Don't Save</button>\n" +
    "    <button type=\"button\" class=\"btn btn-primary\" ng-click=\"ok()\">Save</button>\n" +
    "</div>"
  );

  $templateCache.put("template/widget-default-content.html",
    ""
  );

  $templateCache.put("template/widget-settings-template.html",
    "<div class=\"modal-header\">\n" +
    "    <button type=\"button\" class=\"close\" data-dismiss=\"modal\" aria-hidden=\"true\" ng-click=\"cancel()\">&times;</button>\n" +
    "  <h3>Widget Options <small>{{widget.title}}</small></h3>\n" +
    "</div>\n" +
    "\n" +
    "<div class=\"modal-body\">\n" +
    "    <form name=\"form\" novalidate class=\"form-horizontal\">\n" +
    "        <div class=\"form-group\">\n" +
    "            <label for=\"widgetTitle\" class=\"col-sm-2 control-label\">Title</label>\n" +
    "            <div class=\"col-sm-10\">\n" +
    "                <input type=\"text\" class=\"form-control\" name=\"widgetTitle\" ng-model=\"result.title\">\n" +
    "            </div>\n" +
    "        </div>\n" +
    "        <div ng-if=\"widget.settingsModalOptions.partialTemplateUrl\"\n" +
    "             ng-include=\"widget.settingsModalOptions.partialTemplateUrl\"></div>\n" +
    "    </form>\n" +
    "</div>\n" +
    "\n" +
    "<div class=\"modal-footer\">\n" +
    "    <button type=\"button\" class=\"btn btn-default\" ng-click=\"cancel()\">Cancel</button>\n" +
    "    <button type=\"button\" class=\"btn btn-primary\" ng-click=\"ok()\">OK</button>\n" +
    "</div>"
  );

}]);
