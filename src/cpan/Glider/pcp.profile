alias vi=vim
alias pcp=pcp.sh
alias mkaf=mkaf.sh
alias pmafm=pmafm.sh
alias pmsignal=pmsignal.sh

export PCP_DIR=/c/glider
export PCP_CONF=$PCP_DIR/etc/pcp.conf

PCP_PATH=$PCP_DIR/c/bin
PCP_PATH=$PCP_PATH:$PCP_DIR/bin
PCP_PATH=$PCP_PATH:$PCP_DIR/perl/bin
PCP_PATH=$PCP_PATH:$PCP_DIR/local/bin
export PATH=$PCP_PATH:$PATH
