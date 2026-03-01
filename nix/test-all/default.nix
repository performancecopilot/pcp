# nix/test-all/default.nix
#
# Runs all PCP tests sequentially (container-test, k8s-test, microvm-tests).
# Reports overall pass/fail status.
#
# Usage in flake.nix:
#   testAll = import ./nix/test-all { inherit pkgs lib containerTest k8sTest; };
#
# Generated outputs:
#   testAll.packages.pcp-test-all  - Sequential test runner
#   testAll.apps.pcp-test-all      - App entry point
#
{ pkgs, lib, containerTest, k8sTest }:
let
  # Get the test executables
  containerTestBin = "${containerTest.packages.pcp-container-test}/bin/pcp-container-test";
  k8sTestBin = "${k8sTest.packages.pcp-k8s-test}/bin/pcp-k8s-test";
  minikubeStartBin = "${k8sTest.packages.pcp-minikube-start}/bin/pcp-minikube-start";

  # Import microvm test runner
  microvmTestAll = import ../tests/test-all-microvms.nix { inherit pkgs lib; };
  microvmTestBin = "${microvmTestAll}/bin/pcp-test-all-microvms";

  mkTestAll = pkgs.writeShellApplication {
    name = "pcp-test-all";
    runtimeInputs = with pkgs; [ minikube kubectl bc ];
    text = ''
      set +e  # Don't exit on first failure

      # Colors
      RED='\033[0;31m'
      GREEN='\033[0;32m'
      YELLOW='\033[0;33m'
      BLUE='\033[0;34m'
      CYAN='\033[0;36m'
      BOLD='\033[1m'
      NC='\033[0m'

      # Timing
      time_ms() { date +%s%3N; }
      elapsed_ms() { echo $(( $(time_ms) - $1 )); }
      format_time() {
        local ms=$1
        if [[ $ms -lt 1000 ]]; then
          echo "''${ms}ms"
        elif [[ $ms -lt 60000 ]]; then
          echo "$(echo "scale=1; $ms/1000" | bc)s"
        else
          local mins=$((ms / 60000))
          local secs=$(( (ms % 60000) / 1000 ))
          echo "''${mins}m''${secs}s"
        fi
      }

      # Check if minikube is running
      minikube_running() {
        if timeout 5 minikube status --format='{{.Host}}' 2>/dev/null | grep -q "Running"; then
          return 0
        fi
        return 1
      }

      echo ""
      echo -e "''${BOLD}╔══════════════════════════════════════════════════════════════╗''${NC}"
      echo -e "''${BOLD}║              PCP Test Suite - All Tests                      ║''${NC}"
      echo -e "''${BOLD}╚══════════════════════════════════════════════════════════════╝''${NC}"
      echo ""

      TOTAL_START=$(time_ms)
      TESTS_PASSED=0
      TESTS_FAILED=0
      declare -a FAILED_TESTS=()

      # ─── Prerequisites: Ensure minikube is running ────────────────────────────
      echo -e "''${CYAN}Checking prerequisites...''${NC}"
      if ! minikube_running; then
        echo -e "''${YELLOW}Minikube not running, starting it...''${NC}"
        echo ""
        if "${minikubeStartBin}"; then
          echo ""
          echo -e "''${GREEN}Minikube started successfully''${NC}"
        else
          echo ""
          echo -e "''${RED}Failed to start minikube''${NC}"
          echo -e "''${RED}K8s tests will fail. Continuing anyway...''${NC}"
        fi
      else
        echo -e "''${GREEN}Minikube is running''${NC}"
      fi
      echo ""

      # ─── Test 1: Container Test ─────────────────────────────────────────────
      echo -e "''${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━''${NC}"
      echo -e "''${BOLD}[1/3] Container Test''${NC}"
      echo -e "''${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━''${NC}"
      echo ""

      TEST_START=$(time_ms)
      if "${containerTestBin}"; then
        TEST_ELAPSED=$(elapsed_ms "$TEST_START")
        echo ""
        echo -e "''${GREEN}✓ Container test passed ($(format_time "$TEST_ELAPSED"))''${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
      else
        TEST_ELAPSED=$(elapsed_ms "$TEST_START")
        echo ""
        echo -e "''${RED}✗ Container test failed ($(format_time "$TEST_ELAPSED"))''${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        FAILED_TESTS+=("container-test")
      fi
      echo ""

      # ─── Test 2: Kubernetes Test ────────────────────────────────────────────
      echo -e "''${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━''${NC}"
      echo -e "''${BOLD}[2/3] Kubernetes Test''${NC}"
      echo -e "''${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━''${NC}"
      echo ""

      TEST_START=$(time_ms)
      if "${k8sTestBin}"; then
        TEST_ELAPSED=$(elapsed_ms "$TEST_START")
        echo ""
        echo -e "''${GREEN}✓ Kubernetes test passed ($(format_time "$TEST_ELAPSED"))''${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
      else
        TEST_ELAPSED=$(elapsed_ms "$TEST_START")
        echo ""
        echo -e "''${RED}✗ Kubernetes test failed ($(format_time "$TEST_ELAPSED"))''${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        FAILED_TESTS+=("k8s-test")
      fi
      echo ""

      # ─── Test 3: MicroVM Tests ──────────────────────────────────────────────
      echo -e "''${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━''${NC}"
      echo -e "''${BOLD}[3/3] MicroVM Tests (all variants)''${NC}"
      echo -e "''${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━''${NC}"
      echo ""

      TEST_START=$(time_ms)
      if "${microvmTestBin}" --skip-tap; then
        TEST_ELAPSED=$(elapsed_ms "$TEST_START")
        echo ""
        echo -e "''${GREEN}✓ MicroVM tests passed ($(format_time "$TEST_ELAPSED"))''${NC}"
        TESTS_PASSED=$((TESTS_PASSED + 1))
      else
        TEST_ELAPSED=$(elapsed_ms "$TEST_START")
        echo ""
        echo -e "''${RED}✗ MicroVM tests failed ($(format_time "$TEST_ELAPSED"))''${NC}"
        TESTS_FAILED=$((TESTS_FAILED + 1))
        FAILED_TESTS+=("microvm-tests")
      fi
      echo ""

      # ─── Summary ────────────────────────────────────────────────────────────
      TOTAL_ELAPSED=$(elapsed_ms "$TOTAL_START")
      echo -e "''${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━''${NC}"
      echo -e "''${BOLD}Summary''${NC}"
      echo -e "''${BLUE}━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━━''${NC}"
      echo ""
      echo "  Tests passed: $TESTS_PASSED"
      echo "  Tests failed: $TESTS_FAILED"
      echo "  Total time:   $(format_time "$TOTAL_ELAPSED")"
      echo ""

      if [[ $TESTS_FAILED -eq 0 ]]; then
        echo -e "''${GREEN}''${BOLD}All tests passed!''${NC}"
        echo ""
        exit 0
      else
        echo -e "''${RED}''${BOLD}Failed tests:''${NC}"
        for test in "''${FAILED_TESTS[@]}"; do
          echo -e "  ''${RED}• $test''${NC}"
        done
        echo ""
        exit 1
      fi
    '';
  };

in
{
  # Packages output for flake.nix
  packages = {
    pcp-test-all = mkTestAll;
  };

  # Apps output for flake.nix
  apps = {
    pcp-test-all = {
      type = "app";
      program = "${mkTestAll}/bin/pcp-test-all";
    };
  };
}
