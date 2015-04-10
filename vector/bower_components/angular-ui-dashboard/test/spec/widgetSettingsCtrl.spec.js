'use strict';

describe('Controller: WidgetSettingsCtrl', function() {

    var $scope, widget, tplName, $modalInstance;
    
    beforeEach(module('ui.dashboard'));

    beforeEach(inject(function($rootScope, $controller){
      $scope = $rootScope.$new();
      widget = { title: 'Test Title' };
      tplName = 'some/url/html';
      $modalInstance = {
        close: function() {

        },
        dismiss: function() {

        }
      };
      $controller('WidgetSettingsCtrl', {
          $scope: $scope,
          $modalInstance: $modalInstance,
          widget: widget,
          optionsTemplateUrl: tplName
      });
    }));

    it('should add widget to the dialog scope', function() {
      expect($scope.widget).toEqual(widget);   
    });

    describe('the ok method', function() {
      it('should call close with $scope.result and $scope.widget', function() {
        spyOn($modalInstance, 'close');
        $scope.ok();
        expect($modalInstance.close).toHaveBeenCalled();
        expect($modalInstance.close.calls.argsFor(0)[0] === $scope.result).toEqual(true);
      });
    });

    describe('the cancel method', function() {
      it('should call dismiss', function() {
        spyOn($modalInstance, 'dismiss');
        $scope.cancel();
        expect($modalInstance.dismiss).toHaveBeenCalled();
      });
    });

});