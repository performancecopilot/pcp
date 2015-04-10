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

describe('Directive: dashboard', function () {

  var scope, element, childScope, DashboardState, mockModal, modalOptions, $compile, $q, mockLog;

  // mock UI Sortable
  beforeEach(function () {
    angular.module('ui.sortable', []);
  });

  // load the directive's module
  beforeEach(module('ui.dashboard', function($provide) {
    mockModal = {
      open: function(options) {
        modalOptions = options;
      }
    };
    mockLog = {
      info: function() {

      }
    };
    $provide.value('$modal', mockModal);
    $provide.value('$log', mockLog);
  }));

  beforeEach(inject(function (_$compile_, $rootScope, _DashboardState_, _$q_) {
    // services
    scope = $rootScope.$new();
    $compile = _$compile_;
    DashboardState = _DashboardState_;
    $q = _$q_;

    // options
    var widgetDefinitions = [
      {
        name: 'wt-one',
        template: '<div class="wt-one-value">{{2 + 2}}</div>'
      },
      {
        name: 'wt-two',
        template: '<span class="wt-two-value">{{value}}</span>'
      }
    ];
    var defaultWidgets = _.clone(widgetDefinitions);
    scope.dashboardOptions = {
      widgetButtons: true,
      widgetDefinitions: widgetDefinitions,
      defaultWidgets: defaultWidgets,
      sortableOptions: {
        testProperty: 'foobar'
      }
    };
    scope.value = 10;

    // element setup
    element = $compile('<div dashboard="dashboardOptions"></div>')(scope);
    scope.$digest();
    childScope = element.scope();
  }));

  it('should have toolbar', function () {
    var toolbar = element.find('.btn-toolbar');
    expect(toolbar.length).toEqual(1);
  });

  it('should have UI.Sortable directive', function () {
    var widgetArea = element.find('.dashboard-widget-area');
    expect(widgetArea.attr('ui-sortable')).toBeDefined();
  });

  it('should render widgets', function () {
    var widgets = element.find('.widget');
    expect(widgets.length).toEqual(2);
  });

  it('should evaluate widget expressions', function () {
    var divWidget = element.find('.wt-one-value');
    expect(divWidget.html()).toEqual('4');
  });

  it('should evaluate scope expressions', function () {
    var spanWidget = element.find('.wt-two-value');
    expect(spanWidget.html()).toEqual('10');
  });

  it('should fill options with defaults', function() {
    expect(scope.dashboardOptions.stringifyStorage).toEqual(true);
  });

  it('should not overwrite specified options with defaults', inject(function($compile) {
    scope.dashboardOptions.stringifyStorage = false;
    element = $compile('<div dashboard="dashboardOptions"></div>')(scope);
    $compile(element)(scope);
    scope.$digest();
    expect(scope.dashboardOptions.stringifyStorage).toEqual(false);
  }));

  it('should be able to use a different dashboard template', inject(function($compile, $templateCache) {
    $templateCache.put(
      'myCustomTemplate.html',
        '<div>' +
        '<div ui-sortable="sortableOptions" ng-model="widgets">' +
        '<div ng-repeat="widget in widgets" ng-style="widget.style" class="widget-container custom-widget" widget>' +
        '<h3 class="widget-header">' +
        '{{widget.title}}' +
        '<span ng-click="removeWidget(widget);" class="glyphicon glyphicon-remove" ng-if="!options.hideWidgetClose"></span>' +
        '<span ng-click="openWidgetSettings(widget);" class="glyphicon glyphicon-cog" ng-if="!options.hideWidgetSettings"></span>' +
        '</h3>' +
        '<div class="widget-content"></div>' +
        '<div class="widget-ew-resizer" ng-mousedown="grabResizer($event)"></div>' +
        '</div>' +
        '</div>' +
        '</div>'
    );
    var customElement = $compile('<div dashboard="dashboardOptions" template-url="myCustomTemplate.html"></div>')(scope);
    scope.$digest();
    expect(customElement.find('.custom-widget').length).toEqual(2);
  }));

  it('should set scope.widgets to an empty array if no defaultWidgets are specified', inject(function($compile) {
    delete scope.dashboardOptions.defaultWidgets;
    var element2 = $compile('<div dashboard="dashboardOptions"></div>')(scope);
    scope.$digest();
    var childScope2 = element2.scope();
    expect(childScope2.widgets instanceof Array).toEqual(true);
  }));

  it('should set options.unsavedChangeCount to 0 upon load', function() {
    expect(scope.dashboardOptions.unsavedChangeCount).toEqual(0);
  });

  it('should not call saveDashboard on load', inject(function($compile) {
    spyOn(DashboardState.prototype, 'save');
    var s = scope.$new();
    element = $compile('<div dashboard="dashboardOptions"></div>')(s);
    scope.$digest();
    expect(DashboardState.prototype.save).not.toHaveBeenCalled();
  }));

  describe('the sortableOptions', function() {

    it('should exist', function() {
      expect(typeof childScope.sortableOptions).toEqual('object');
    });

    it('should be possible to be extendable from the dashboardOptions', function() {
      expect(childScope.sortableOptions.testProperty).toEqual('foobar');
    })

    it('should have a stop function that calls $scope.saveDashboard', function() {
      expect(typeof childScope.sortableOptions.stop).toEqual('function');
      spyOn(childScope, 'saveDashboard');
      childScope.sortableOptions.stop();
      expect(childScope.saveDashboard).toHaveBeenCalled();
    });
  });

  describe('the addWidget function', function() {

    var widgetCreated, widgetPassed, widgetDefault;

    beforeEach(function() {
      childScope.widgets.push = function(w) {
        widgetCreated = w;
      }
    });

    it('should be a function', function() {
      expect(typeof childScope.addWidget).toEqual('function');
    });

    it('should throw if no default widgetDefinition was found', function() {
      spyOn(childScope.widgetDefs, 'getByName').and.returnValue(false);
      function fn () {
        childScope.addWidget({ name: 'notReal' });
      }
      expect(fn).toThrow();
    });

    it('should look to the passed widgetToInstantiate object for the title before anything else', function() {
      spyOn(childScope.widgetDefs, 'getByName').and.returnValue({ title: 'defaultTitle', name: 'A' });
      childScope.addWidget({ title: 'highestPrecedence', name: 'A' });
      expect(widgetCreated.title).toEqual('highestPrecedence');
    });

    it('should use the defaultWidget\'s title second', function() {
      spyOn(childScope.widgetDefs, 'getByName').and.returnValue({ title: 'defaultTitle', name: 'A' });
      childScope.addWidget({ name: 'A' });
      expect(widgetCreated.title).toEqual('defaultTitle');
    });

    it('should call the saveDashboard method (internal)', function() {
      spyOn(childScope.widgetDefs, 'getByName').and.returnValue({ title: 'defaultTitle', name: 'A' });
        spyOn(childScope, 'saveDashboard');
        childScope.addWidget({ name: 'A' });
        expect(childScope.saveDashboard).toHaveBeenCalled();
    });

    describe('@awashbrook Test Case', function() {
      beforeEach(function() {
        spyOn(childScope.widgetDefs, 'getByName').and.returnValue(widgetDefault = {
          "name": "nvLineChartAlpha",
          "directive": "nvd3-line-chart",
          "dataAttrName": "data",
          "attrs": {
            "isArea": true,
            "height": 400,
            "showXAxis": true,
            "showYAxis": true,
            "xAxisTickFormat": "xAxisTickFormat()",
            "interactive": true,
            "useInteractiveGuideline": true,
            "tooltips": true,
            "showLegend": true,
            "noData": "No data for YOU!",
            "color": "colorFunction()",
            "forcey": "[0,2]"
          },
          "dataModelOptions": {
            "params": {
              "from": "-2h",
              "until": "now"
            }
          },
          "style": {
            "width": "400px"
          },
        });
        childScope.addWidget(widgetPassed = {
          "title": "Andy",
          "name": "nvLineChartAlpha",
          "style": {
            "width": "400px"
          },
          "dataModelOptions": {
            "params": {
              "from": "-1h",
              "target": [
              "randomWalk(\"random Andy 1\")",
              "randomWalk(\"random walk 2\")",
              "randomWalk(\"random walk 3\")"
              ]
            }
          },
          "attrs": {
            "height": 400,
            "showXAxis": true,
            "showYAxis": true,
            "xAxisTickFormat": "xAxisTickFormat()",
            "interactive": false,
            "useInteractiveGuideline": true,
            "tooltips": true,
            "showLegend": true,
            "noData": "No data for YOU!",
            "color": "colorFunction()",
            "forcey": "[0,2]",
            "data": "widgetData"
          }
        });
      });

      it('should keep overrides from widgetPassed', function() {
        expect(widgetCreated.attrs.interactive).toEqual(widgetPassed.attrs.interactive);
      });

      it('should fill in default attrs', function() {
        expect(widgetCreated.attrs.isArea).toEqual(widgetDefault.attrs.isArea);
      });

      it('should override deep options in dataModelOptions', function() {
        expect(widgetCreated.dataModelOptions.params.from).toEqual(widgetPassed.dataModelOptions.params.from);
      });

      it('should fill in deep default attrs', function() {
        expect(widgetCreated.dataModelOptions.params.until).toEqual(widgetDefault.dataModelOptions.params.until);
      });
    });

    describe('the doNotSave parameter', function() {

      it('should prevent save from being called if set to true', function() {
        spyOn(childScope.widgetDefs, 'getByName').and.returnValue({ title: 'defaultTitle', name: 'A' });
        spyOn(childScope, 'saveDashboard');
        childScope.addWidget({ name: 'A' }, true);
        expect(childScope.saveDashboard).not.toHaveBeenCalled();
      });

    });

  });

  describe('the removeWidget function', function() {

    it('should be a function', function() {
      expect(typeof childScope.removeWidget).toEqual('function');
    });

    it('should remove the provided widget from childScope.widgets array', function() {
      var startingLength = childScope.widgets.length;
      var expectedLength = startingLength - 1;

      var widgetToRemove = childScope.widgets[0];
      childScope.removeWidget(widgetToRemove);

      expect(childScope.widgets.length).toEqual(expectedLength);
      expect(childScope.widgets.indexOf(widgetToRemove)).toEqual(-1);
    });

    it('should call saveDashboard', function() {
      spyOn(childScope, 'saveDashboard');
      var widgetToRemove = childScope.widgets[0];
      childScope.removeWidget(widgetToRemove);
      expect(childScope.saveDashboard).toHaveBeenCalled();
    });

  });

  describe('the saveDashboard function', function() {

    it('should be attached to the options object after initialization', function() {
      expect(typeof scope.dashboardOptions.saveDashboard).toEqual('function');
      expect(scope.dashboardOptions.saveDashboard === childScope.externalSaveDashboard).toEqual(true);
    });

    it('should call scope.dashboardState.save when called internally if explicitSave is falsey', function() {
      spyOn(childScope.dashboardState, 'save').and.returnValue(true);
      childScope.saveDashboard();
      expect(childScope.dashboardState.save).toHaveBeenCalled();
    });

    it('should not call scope.dashboardState.save when called internally if explicitSave is truthy', function() {
      scope.dashboardOptions.explicitSave = true;
      spyOn(childScope.dashboardState, 'save').and.returnValue(true);
      childScope.saveDashboard();
      expect(childScope.dashboardState.save).not.toHaveBeenCalled();
    });

    it('should call scope.dashboardState.save when called externally, no matter what explicitSave value is', function() {
      spyOn(childScope.dashboardState, 'save').and.returnValue(true);

      scope.dashboardOptions.explicitSave = false;
      scope.dashboardOptions.saveDashboard();
      expect(childScope.dashboardState.save.calls.count()).toEqual(1);

      scope.dashboardOptions.explicitSave = true;
      scope.dashboardOptions.saveDashboard();
      expect(childScope.dashboardState.save.calls.count()).toEqual(2);
    });

    it('should keep a count of unsaved changes as unsavedChangeCount', function() {
      scope.dashboardOptions.explicitSave = true;
      spyOn(childScope.dashboardState, 'save').and.returnValue(true);
      childScope.saveDashboard();
      expect(scope.dashboardOptions.unsavedChangeCount).toEqual(1);
      childScope.saveDashboard();
      childScope.saveDashboard();
      expect(scope.dashboardOptions.unsavedChangeCount).toEqual(3);
    });

    it('should reset the cound of unsaved changes if a successful force save occurs', function() {
      scope.dashboardOptions.explicitSave = true;
      spyOn(childScope.dashboardState, 'save').and.returnValue(true);

      childScope.saveDashboard();
      childScope.saveDashboard();
      childScope.saveDashboard();

      childScope.saveDashboard(true);

      expect(scope.dashboardOptions.unsavedChangeCount).toEqual(0);
    });

  });

  describe('the loadWidgets function', function() {

    it('should be a function', function() {
      expect(typeof childScope.loadWidgets).toEqual('function');
    });

    it('should set savedWidgetDefs on scope as passed array', function() {
      var widgets = [];
      childScope.loadWidgets(widgets);
      expect(childScope.savedWidgetDefs === widgets).toEqual(true);
    });

    it('should call clear on the scope with true as the only argument', function() {
      spyOn(childScope, 'clear');
      childScope.loadWidgets([]);
      expect(childScope.clear).toHaveBeenCalled();
      expect(childScope.clear.calls.argsFor(0)).toEqual([true]);
    });

    it('should call addWidget for each widget in the array', function() {
      spyOn(childScope, 'addWidget').and.returnValue(null);
      var widgets = [{},{},{}];
      childScope.loadWidgets(widgets);
      expect(childScope.addWidget.calls.count()).toEqual(3);
    });

    it('should call addWidget for each widget with true as the second parameter (doNotSave)', function() {
      spyOn(childScope, 'addWidget').and.returnValue(null);
      var widgets = [{},{},{}];
      childScope.loadWidgets(widgets);
      expect(childScope.addWidget.calls.argsFor(0)).toEqual( [ widgets[0], true] );
      expect(childScope.addWidget.calls.argsFor(1)).toEqual( [ widgets[1], true] );
      expect(childScope.addWidget.calls.argsFor(2)).toEqual( [ widgets[2], true] );
    });

  });

  describe('the clear function', function() {

    it('should set the scope to an empty array', function() {
      childScope.clear();
      expect(childScope.widgets).toEqual([]);
    });

    it('should not call saveDashboard if first arg is true', function() {
      spyOn(childScope, 'saveDashboard');
      childScope.clear(true);
      expect(childScope.saveDashboard).not.toHaveBeenCalled();
    });

    it('should call saveDashboard if first arg is not true', function() {
      spyOn(childScope, 'saveDashboard');
      childScope.clear();
      expect(childScope.saveDashboard).toHaveBeenCalled();
    });

  });

  describe('the openWidgetSettings function', function() {

    it('should be a function', function() {
      expect(typeof childScope.openWidgetSettings).toEqual('function');
    });

    it('should call $modal.open with default options', function() {
      var widget = {};
      spyOn(mockModal, 'open').and.returnValue({
        result: { then: function(fn) {} }
      });
      childScope.openWidgetSettings(widget);
      expect(mockModal.open).toHaveBeenCalled();
    });

    it('should have widget in the resolve object', function() {
      var widget = {};
      var dfr = $q.defer();
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });
      childScope.openWidgetSettings(widget);
      expect(modalOptions.resolve.widget() === widget).toEqual(true);
    });

    it('should set the templateUrl in modal options to the default ("template/widget-settings-template.html")', function() {
      var widget = {};
      var dfr = $q.defer();
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });
      childScope.openWidgetSettings(widget);
      expect(modalOptions.templateUrl).toEqual('template/widget-settings-template.html');
    });

    it('should set the templateUrl in modal options to scope.options.settingsModalOptions.templateUrl', function() {
      var other;
      scope.dashboardOptions.settingsModalOptions = {
        templateUrl: other = 'some/other/url.html'
      };
      var widget = {};
      var dfr = $q.defer();
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });
      childScope.openWidgetSettings(widget);
      expect(modalOptions.templateUrl).toEqual(other);
    });

    it('should set the templateUrl in modal options to widget.settingsModalOptions.templateUrl, if present', function() {
      var expected;
      var widget = {
        settingsModalOptions: {
          templateUrl: expected = 'specific/template.html'
        }
      };
      var dfr = $q.defer();
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });
      childScope.openWidgetSettings(widget);
      expect(modalOptions.templateUrl).toEqual(expected);
    });

    it('should set the controller in modal options to the default ("WidgetSettingsCtrl")', function() {
      var widget = {};
      var dfr = $q.defer();
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });
      childScope.openWidgetSettings(widget);
      expect(modalOptions.controller).toEqual('WidgetSettingsCtrl');
    });

    it('should set the controller in modal options to the default ("WidgetSettingsCtrl"), even when settingsModalOptions is supplied in options', inject(function($rootScope) {
      
      scope = $rootScope.$new();

      // options
      var widgetDefinitions = [
        {
          name: 'wt-one',
          template: '<div class="wt-one-value">{{2 + 2}}</div>'
        },
        {
          name: 'wt-two',
          template: '<span class="wt-two-value">{{value}}</span>'
        }
      ];
      var defaultWidgets = _.clone(widgetDefinitions);
      scope.dashboardOptions = {
        widgetButtons: true,
        widgetDefinitions: widgetDefinitions,
        defaultWidgets: defaultWidgets,
        sortableOptions: {
          testProperty: 'foobar'
        },
        settingsModalOptions: {
          backdrop: false
        }
      };
      scope.value = 10;

      var dfr = $q.defer();
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });

      // element setup
      element = $compile('<div dashboard="dashboardOptions"></div>')(scope);
      scope.$digest();
      childScope = element.scope();

      childScope.openWidgetSettings({});
      expect(modalOptions.controller).toEqual('WidgetSettingsCtrl');

    }));

    it('should set the controller in modal options to the default ("WidgetSettingsCtrl"), even when settingsModalOptions is supplied in widget', inject(function($rootScope) {
      
      scope = $rootScope.$new();

      // options
      var widgetDefinitions = [
        {
          name: 'wt-one',
          template: '<div class="wt-one-value">{{2 + 2}}</div>'
        },
        {
          name: 'wt-two',
          template: '<span class="wt-two-value">{{value}}</span>'
        }
      ];
      var defaultWidgets = _.clone(widgetDefinitions);
      scope.dashboardOptions = {
        widgetButtons: true,
        widgetDefinitions: widgetDefinitions,
        defaultWidgets: defaultWidgets,
        sortableOptions: {
          testProperty: 'foobar'
        },
        settingsModalOptions: {
          backdrop: false
        }
      };
      scope.value = 10;

      var dfr = $q.defer();
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });

      // element setup
      element = $compile('<div dashboard="dashboardOptions"></div>')(scope);
      scope.$digest();
      childScope = element.scope();

      childScope.openWidgetSettings({
        settingsModalOptions: {
          templateUrl: 'custom/widget/template.html'
        }
      });
      expect(modalOptions.controller).toEqual('WidgetSettingsCtrl');
      expect(modalOptions.backdrop).toEqual(false);
      expect(modalOptions.templateUrl).toEqual('custom/widget/template.html');

    }));

    it('should set the controller to scope.options.settingsModalOptions.controller if provided', function() {
      scope.dashboardOptions.settingsModalOptions = {};
      var expected = scope.dashboardOptions.settingsModalOptions.controller = 'MyCustomCtrl';
      var widget = {};
      var dfr = $q.defer();
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });
      childScope.openWidgetSettings(widget);
      expect(modalOptions.controller).toEqual(expected);
    });

    it('should set the controller to widget.settingsModalOptions.controller if provided', function() {
      var expected;
      var widget = {
        settingsModalOptions: {
          controller: expected = 'MyWidgetCtrl'
        }
      };
      var dfr = $q.defer();
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });
      childScope.openWidgetSettings(widget);
      expect(modalOptions.controller).toEqual(expected);
    });

    it('should pass in other modal options set in scope.options.settingsModalOptions', function() {
      scope.dashboardOptions.settingsModalOptions = {
        keyboard: false,
        windowClass: 'my-extra-class'
      };
      var widget = {};
      var dfr = $q.defer();
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });
      childScope.openWidgetSettings(widget);
      expect(modalOptions.keyboard).toEqual(false);
      expect(modalOptions.windowClass).toEqual('my-extra-class');
    });

    it('should pass in other modal options set in widget.settingsModalOptions', function() {
      scope.dashboardOptions.settingsModalOptions = {
        keyboard: false,
        windowClass: 'my-extra-class'
      };
      var widget = {
        settingsModalOptions: {
          keyboard: true,
          size: 'sm'
        }
      };
      var dfr = $q.defer();
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });
      childScope.openWidgetSettings(widget);
      expect(modalOptions.keyboard).toEqual(true);
      expect(modalOptions.size).toEqual('sm');
      expect(modalOptions.windowClass).toEqual('my-extra-class');
    });

    it('should emit a "widgetChanged" event on the childScope when the modal promise is called', function(done) {
      var widget = {};
      var result = {};
      var dfr = $q.defer();
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });
      spyOn(childScope.options, 'onSettingsClose');
      childScope.openWidgetSettings(widget);
      childScope.$on('widgetChanged', done);
      dfr.resolve(result, widget);
      childScope.$digest();
    });

    it('should call scope.options.onSettingsClose when the modal promise is resolved by default', function() {
      var widget = {};
      var result = {};
      var dfr = $q.defer();
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });
      spyOn(childScope.options, 'onSettingsClose');
      childScope.openWidgetSettings(widget);
      dfr.resolve(result);
      childScope.$digest();
      expect(scope.dashboardOptions.onSettingsClose).toHaveBeenCalledWith(result, widget, childScope);
    });

    it('should call scope.options.onSettingsDismiss when the modal promise is rejected by default', function() {
      var widget = {};
      var result = {};
      var dfr = $q.defer();
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });
      spyOn(childScope.options, 'onSettingsDismiss');
      childScope.openWidgetSettings(widget);
      dfr.reject('Testing failure');
      childScope.$digest();
      expect(scope.dashboardOptions.onSettingsDismiss).toHaveBeenCalledWith('Testing failure', childScope);
    });

    it('should call widget.onSettingsClose if provided when the modal promise is resolved', function() {
      var widget = {
        onSettingsClose: function(result, widget, scope) {

        }
      };
      var result = {};
      var dfr = $q.defer();
      spyOn(widget, 'onSettingsClose');
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });
      spyOn(childScope.options, 'onSettingsClose');
      childScope.openWidgetSettings(widget);
      dfr.resolve(result);
      childScope.$digest();
      expect(scope.dashboardOptions.onSettingsClose).not.toHaveBeenCalled();
      expect(widget.onSettingsClose).toHaveBeenCalledWith(result, widget, childScope);
    });

    it('should call widget.onSettingsDismiss if provided when the modal promise is rejected', function() {
      var widget = {
        onSettingsDismiss: function(result, widget, scope) {

        }
      };
      var result = {};
      var dfr = $q.defer();
      spyOn(widget, 'onSettingsDismiss');
      spyOn(mockModal, 'open').and.callFake(function(options) {
        modalOptions = options;
        return {
          result: dfr.promise
        };
      });
      spyOn(childScope.options, 'onSettingsDismiss');
      childScope.openWidgetSettings(widget);
      dfr.reject('Testing failure');
      childScope.$digest();
      expect(scope.dashboardOptions.onSettingsDismiss).not.toHaveBeenCalled();
      expect(widget.onSettingsDismiss).toHaveBeenCalledWith('Testing failure', childScope);
    });

  });

  describe('the default onSettingsClose callback', function() {

    var onSettingsClose;

    beforeEach(function() {
      onSettingsClose = childScope.options.onSettingsClose;
    });
    
    it('should exist', function() {
      expect(typeof onSettingsClose).toEqual('function');
    });

    it('should deep extend widget with result', function() {
      var result = {
        title: 'andy',
        style: {
          'float': 'left'
        }
      };
      var widget = {
        title: 'scott',
        style: {
          width: '100px'
        }
      };
      onSettingsClose(result, widget, {});
      expect(widget).toEqual({
        title: 'andy',
        style: {
          width: '100px',
          'float': 'left'
        }
      });
    });

  });

  describe('the default onSettingsDismiss callback', function() {
    
    var onSettingsDismiss;

    beforeEach(function() {
      onSettingsDismiss = childScope.options.onSettingsDismiss;
    });

    it('should exist', function() {
      expect(typeof onSettingsDismiss).toEqual('function');
    });

    it('should call $log.info with the reason', function() {
      spyOn(mockLog, 'info');
      onSettingsDismiss('dismiss reason');
      expect(mockLog.info).toHaveBeenCalled();
    });

  });

});
