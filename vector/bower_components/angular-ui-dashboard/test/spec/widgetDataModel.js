'use strict';

describe('Factory: WidgetDataModel', function () {

  // load the service's module
  beforeEach(module('ui.dashboard'));

  // instantiate service
  var WidgetDataModel, m, widget, scope;

  beforeEach(inject(function (_WidgetDataModel_) {
    WidgetDataModel = _WidgetDataModel_;
    m = new WidgetDataModel();
    widget = {
      dataAttrName: 'testing',
      dataModelOptions: { opt: true }
    };
    scope = {
      fake: 'scope'
    };
  }));

  it('should be a function', function() {
    expect(typeof WidgetDataModel).toEqual('function');
  });

  describe('setup method', function() {

    it('should set dataAttrName, dataModelOptions, and widgetScope from args', function() {
      m.setup(widget, scope);
      expect(m.dataAttrName).toEqual(widget.dataAttrName);
      expect(m.dataModelOptions).toEqual(widget.dataModelOptions);
      expect(m.widgetScope).toEqual(scope);
    });

  });

  describe('updateScope method', function() {
    
    it('should set scope.widgetData to passed data', function() {
      m.setup(widget, scope);
      var newData = [];
      m.updateScope(newData);
      expect(scope.widgetData).toEqual(newData);
    });

  });

  describe('init method', function() {
    it('should be an empty (noop) implementation', function() {
      expect(typeof m.init).toEqual('function');
      expect(m.init).not.toThrow();
    });
  });

  describe('destroy method', function() {
    it('should be an empty (noop) implementation', function() {
      expect(typeof m.destroy).toEqual('function');
      expect(m.destroy).not.toThrow();
    });
  });

});