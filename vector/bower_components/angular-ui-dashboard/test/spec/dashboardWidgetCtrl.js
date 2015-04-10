'use strict';

describe('Controller: DashboardWidgetCtrl', function() {

  var $scope, $element, $timeout, injections;

  beforeEach(module('ui.dashboard'));

  beforeEach(inject(function($rootScope, $controller){
    $scope = $rootScope.$new();
    $element = angular.element('<div><div class="widget"></div></div>');
    $timeout = function timeout(fn) {
      fn();
    };
    injections = {
      $scope: $scope,
      $element: $element,
      $timeout: $timeout
    };
    spyOn(injections, '$timeout');
    $controller('DashboardWidgetCtrl', injections);
  }));

  describe('the makeTemplateString method', function() {

    it('should return a string', function() {
      $scope.widget = {
        templateUrl: 'some/template.html'
      };
      expect(typeof $scope.makeTemplateString()).toEqual('string');
    });

    it('should use ng-include if templateUrl is specified on widget, despite any other options', function() {
      $scope.widget = {
        templateUrl: 'some/template.html',
        template: 'not this one',
        directive: 'or-this',
        attrs: {
          something: 'awesome',
          other: 'thing'
        }
      };
      expect($scope.makeTemplateString()).toMatch(/ng-include="'some\/template\.html'"/);
    });

    it('should return widget.template if specified, regardless of presence of directive or attrs', function() {
      $scope.widget = {
        template: '<div class="testing"></div>',
        directive: 'no-good'
      };
      expect($scope.makeTemplateString()).toEqual($scope.widget.template);
    });

    it('should use widget.directive as attribute directive', function() {
      $scope.widget = {
        directive: 'ng-awesome'
      };
      expect($scope.makeTemplateString()).toEqual('<div ng-awesome></div>');
    });

    it('should attach attributes if provided', function() {
      $scope.widget = {
        directive: 'ng-awesome',
        attrs: {
          'ng-awesome': 'test1',
          other: 'attr',
          more: 'stuff'
        }
      };
      expect($scope.makeTemplateString()).toEqual('<div ng-awesome="test1" other="attr" more="stuff"></div>');
    });

    it('should place widgetData into dataAttrName attribute if specified', function() {
      $scope.widget = {
        directive: 'ng-awesome',
        attrs: {
          'ng-awesome': 'test1',
          other: 'attr',
          more: 'stuff'
        },
        dataAttrName: 'data'
      };
      expect($scope.makeTemplateString()).toEqual('<div ng-awesome="test1" other="attr" more="stuff" data="widgetData"></div>');
    });

    it('should add attrs to the widget object if it does not exist and dataAttrName is specified', function() {
      $scope.widget = {
        directive: 'ng-awesome',
        dataAttrName: 'data'
      };
      expect($scope.makeTemplateString()).toEqual('<div ng-awesome data="widgetData"></div>');
    });

  });

  describe('the grabResizer method', function() {

    var evt, widget, WidgetModel;

    beforeEach(inject(function (_WidgetModel_) {
      WidgetModel = _WidgetModel_;
    }));

    beforeEach(function() {
      evt = {
        stopPropagation: jasmine.createSpy('stopPropagation'),
        originalEvent: {
          preventDefault: jasmine.createSpy('preventDefault')
        },
        clientX: 100,
        which: 1
      };
      $scope.widget = widget = new WidgetModel({
        style: {
          width: '30%'
        }
      });
    });

    it('should do nothing if event.which is not 1 (left click)', function() {
      evt.which = 2;
      $scope.grabResizer(evt);
      expect(evt.stopPropagation).not.toHaveBeenCalled();
    });

    it('should call stopPropagation and preventDefault', function() {
      $scope.grabResizer(evt);
      expect(evt.stopPropagation).toHaveBeenCalled();
      expect(evt.originalEvent.preventDefault).toHaveBeenCalled();
    });

    it('should add a .widget-resizer-marquee element to the .widget element', function() {
      $scope.grabResizer(evt);
      expect($element.find('.widget-resizer-marquee').length).toBeGreaterThan(0);
    });

  });

  describe('the editTitle method', function() {
    
    it('should set editingTitle=true on the widget object', function() {
      var widget = {};
      $scope.editTitle(widget);
      expect(widget.editingTitle).toEqual(true);      
    });

    it('should call $timeout', function() {
      var widget = {};
      $scope.editTitle(widget);
      expect(injections.$timeout).toHaveBeenCalled();
    });

  });

  describe('the saveTitleEdit method', function() {
    
    it('should set editingTitle=false', function() {
      var widget = { editingTitle: true };
      $scope.saveTitleEdit(widget);
      expect(widget.editingTitle).toEqual(false);
    });
  });

});