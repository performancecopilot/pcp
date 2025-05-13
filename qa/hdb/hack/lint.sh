#!/usr/bin/env bash
SCRIPT_DIR="$( cd -- "$( dirname -- "${BASH_SOURCE[0]}" )" &> /dev/null && pwd )"
PROJECT_ROOT=$SCRIPT_DIR/..

printf "\n############## isort ##############\n"
isort "$PROJECT_ROOT"

printf "\n############## black ##############\n"
black "$PROJECT_ROOT"

printf "\n############## mypy ##############\n"
mypy --ignore-missing-imports "$PROJECT_ROOT"

printf "\n############## flake8 ##############\n"
flake8 --ignore=E501 "$PROJECT_ROOT"

printf "\n############## pylint ##############\n"
pylint "$PROJECT_ROOT/pmdahdb.py"

printf "\n############## shellcheck ##############\n"
shellcheck "$PROJECT_ROOT"/test/*.sh
shellcheck "$PROJECT_ROOT"/hack/*.sh
