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