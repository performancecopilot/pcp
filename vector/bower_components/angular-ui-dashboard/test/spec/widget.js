// 'use strict';

describe('Directive: widget', function () {

  var element, scope, rootScope, isoScope, compile, provide;

  function Type() {
  }

  Type.prototype = {
    setup: function () {
    },
    init: function () {
    },
    destroy: function () {
    }
  };

  beforeEach(function () {
    spyOn(Type.prototype, 'setup');
    spyOn(Type.prototype, 'init');
    spyOn(Type.prototype, 'destroy');
    // define mock objects here
  });

  // load the directive's module
  beforeEach(module('ui.dashboard', function ($provide, $controllerProvider) {
    provide = $provide;
    // Inject dependencies like this:
    $controllerProvider.register('DashboardWidgetCtrl', function ($scope) {

    });

  }));

  beforeEach(inject(function ($compile, $rootScope) {
    // Cache these for reuse
    rootScope = $rootScope;
    compile = $compile;

    // Other setup, e.g. helper functions, etc.

    // Set up the outer scope
    scope = $rootScope.$new();
    scope.widget = {
      dataModelType: Type
    };

    compileTemplate = jasmine.createSpy('compileTemplate');
    scope.compileTemplate = compileTemplate;
  }));

  function compileWidget() {
    // Define and compile the element
    element = angular.element('<div widget><div class="widget-content"></div></div>');
    element = compile(element)(scope);
    scope.$digest();
    isoScope = element.isolateScope();
  }

  it('should create a new instance of dataModelType if provided in scope.widget', function () {
    compileWidget();
    expect(scope.widget.dataModel instanceof Type).toBe(true);
  });

  it('should call setup and init on the new dataModel', function () {
    compileWidget();
    expect(Type.prototype.setup).toHaveBeenCalled();
    expect(Type.prototype.init).toHaveBeenCalled();
  });

  it('should call compile template', function () {
    compileWidget();
    expect(scope.compileTemplate).toHaveBeenCalled();
  });

  it('should create a new instance of dataModelType from string name', function () {
    // register data model with $injector
    provide.factory('StringNameDataModel', function () {
      return Type;
    });

    scope.widget = {
      dataModelType: 'StringNameDataModel'
    };

    compileWidget();

    expect(scope.widget.dataModel instanceof Type).toBe(true);
    expect(Type.prototype.setup).toHaveBeenCalled();
    expect(Type.prototype.init).toHaveBeenCalled();
  });

  it('should validate data model type', function () {
    scope.widget = {
      dataModelType: {}
    };

    expect(function () {
      compileWidget()
    }).toThrowError();
  });

});