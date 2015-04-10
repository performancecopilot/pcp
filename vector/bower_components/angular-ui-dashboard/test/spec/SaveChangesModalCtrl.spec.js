'use strict';

describe('Controller: SaveChangesModalCtrl', function() {

  var $scope, $mockModalInstance, layout;

  beforeEach(module('ui.dashboard'));

  beforeEach(inject(function($rootScope, $controller){
    $scope = $rootScope.$new();
    $mockModalInstance = {
      close: function() {},
      dismiss: function() {}
    };
    layout = {};
    $controller('SaveChangesModalCtrl', {
      $scope: $scope,
      $modalInstance: $mockModalInstance,
      layout: layout
    });

  }));

  it('should set the injected layout to scope.layout', function() {
    expect($scope.layout === layout).toEqual(true)
  });

  describe('the ok method', function() {
    it('should be a function', function() {
      expect(typeof $scope.ok).toEqual('function');
    });
    it('should call close', function() {
      spyOn($mockModalInstance, 'close');
      $scope.ok();
      expect($mockModalInstance.close).toHaveBeenCalled();
    });
  });

  describe('the cancel function', function() {
    it('should be a function', function() {
      expect(typeof $scope.cancel).toEqual('function'); 
    });
    it('should call dismiss', function() {
      spyOn($mockModalInstance, 'dismiss');
      $scope.cancel();
      expect($mockModalInstance.dismiss).toHaveBeenCalled();
    });
  });

});