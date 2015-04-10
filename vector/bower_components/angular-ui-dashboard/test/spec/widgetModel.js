'use strict';

describe('Factory: WidgetModel', function () {

  // load the service's module
  beforeEach(module('ui.dashboard'));

  // instantiate service
  var WidgetModel;
  beforeEach(inject(function (_WidgetModel_) {
    WidgetModel = _WidgetModel_;
  }));

  it('should be a function', function() {
    expect(typeof WidgetModel).toEqual('function');
  });

  describe('the constructor', function() {
    var m, Class, Class2, overrides;

    beforeEach(function() {
      Class = {
        name: 'TestWidget', 
        attrs: {},
        dataAttrName: 'attr-name',
        dataModelType: function TestType() {},
        dataModelOptions: {},
        style: { width: '10em' },
        settingsModalOptions: {},
        onSettingsClose: function() {},
        onSettingsDismiss: function() {}
      };

      Class2 = {
        name: 'TestWidget2',
        attrs: {},
        dataAttrName: 'attr-name',
        dataModelType: function TestType() {},
        dataModelOptions: {},
        style: { width: '10em' },
        templateUrl: 'my/url.html',
        template: '<div>some template</div>'
      };

      overrides = {
        style: {
          width: '15em'
        }
      };
      spyOn(WidgetModel.prototype, 'setWidth');
      m = new WidgetModel(Class, overrides);
    });

    it('should copy class defaults, so that changes on an instance do not change the Class', function() {
      m.style.width = '20em';
      expect(Class.style.width).toEqual('10em');
    });

    it('should call setWidth', function() {
      expect(WidgetModel.prototype.setWidth).toHaveBeenCalled();
    });

    it('should take overrides as precedent over Class defaults', function() {
      expect(m.style.width).toEqual('15em');
    });

    it('should set templateUrl if and only if it is present on Class', function() {
      var m2 = new WidgetModel(Class2, overrides);
      expect(m2.templateUrl).toEqual('my/url.html');
    });

    it('should NOT set template if templateUrl was specified', function() {
      var m2 = new WidgetModel(Class2, overrides);
      expect(m2.template).toBeUndefined();
    });

    it('should set template if and only if it is present on Class', function() {
      delete Class2.templateUrl;
      var m2 = new WidgetModel(Class2, overrides);
      expect(m2.template).toEqual('<div>some template</div>');
    });

    it('should look for directive if neither templateUrl nor template is found on Class', function() {
      delete Class2.templateUrl;
      delete Class2.template;
      Class2.directive = 'ng-bind';
      var m2 = new WidgetModel(Class2, overrides);
      expect(m2.directive).toEqual('ng-bind');
    });

    it('should set the name as directive if templateUrl, template, and directive are not defined', function() {
      delete Class2.templateUrl;
      delete Class2.template;
      var m2 = new WidgetModel(Class2, overrides);
      expect(m2.directive).toEqual('TestWidget2');
    });

    it('should not require overrides', function() {
      var fn = function() {
        var m2 = new WidgetModel(Class);
      }
      expect(fn).not.toThrow();
    });

    it('should copy references to settingsModalOptions, onSettingsClose, onSettingsDismiss', function() {
      var m = new WidgetModel(Class);
      expect(m.settingsModalOptions).toEqual(Class.settingsModalOptions);
      expect(m.onSettingsClose).toEqual(Class.onSettingsClose);
      expect(m.onSettingsDismiss).toEqual(Class.onSettingsDismiss);
    });

  });

  describe('setWidth method', function() {

    var context, setWidth;

    beforeEach(function() {
      context = new WidgetModel({});
      setWidth = WidgetModel.prototype.setWidth;
    });
    
    it('should take one argument as a string with units', function() {
      setWidth.call(context, '100px');
      expect(context.containerStyle.width).toEqual('100px');
    });

    it('should take two args as a number and string as units', function() {
      setWidth.call(context, 100, 'px');
      expect(context.containerStyle.width).toEqual('100px');
    });

    it('should return false and not set anything if width is less than 0', function() {
      var result = setWidth.call(context, -100, 'em');
      expect(result).toEqual(false);
      expect(context.containerStyle.width).not.toEqual('-100em');
    });

    it('should assume % if no unit is given', function() {
      setWidth.call(context, 50);
      expect(context.containerStyle.width).toEqual('50%');
    });

    it('should force greater than 0% and less than or equal 100%', function() {
      setWidth.call(context, '110%');
      expect(context.containerStyle.width).toEqual('100%');
    });

  });

});