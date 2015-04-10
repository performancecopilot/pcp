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