alias vi=vim
export PCP_DIR=/c/glider
export PCP_CONF=$PCP_DIR/etc/pcp.conf
export PCP_CONFIG=$PCP_DIR/local/bin/pmconfig.exe

PCP_PATH=$PCP_DIR/c/bin
PCP_PATH=$PCP_PATH:$PCP_DIR/bin
PCP_PATH=$PCP_PATH:$PCP_DIR/perl/bin
PCP_PATH=$PCP_PATH:$PCP_DIR/local/bin
export PATH=$PCP_PATH:$PATH
