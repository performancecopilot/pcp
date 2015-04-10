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