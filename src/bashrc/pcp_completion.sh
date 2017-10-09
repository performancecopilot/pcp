_pcp_complete()
{
    # All supported arguments for a command
    local all_args=""
    # Regex for args expecting non-metric parameter
    local arg_regex=""

    local cmd=${COMP_WORDS[0]}
    local cur=${COMP_WORDS[$COMP_CWORD]}

    COMPREPLY=()

    # Register arguments
    case $cmd in
    pcp2elasticsearch)
        all_args="ahLKcCeVHGASTOstrIivPqbygXx"
        arg_regex="-[ahKceASTOstZiPqbygXx]"
    ;;
    pcp2json)
        all_args="ahLKcCeVHGASTOstZzrIivPqbyFfXx"
        arg_regex="-[ahKceASTOstZiPqbygFf]"
    ;;
    pcp2xlsx)
        all_args="ahLKcCeVHGASTOstZzrIivPqbyFf"
        arg_regex="-[ahKceASTOstZiPqbygFf]"
    ;;
    pcp2xml)
        all_args="ahLKcCeVHGASTOstZzrIivPqbyFfXx"
        arg_regex="-[ahKceASTOstZiPqbygFf]"
    ;;
    pcp2zabbix)
        all_args="ahLKcCeVHGASTOstrIivPqbygpXEx"
        arg_regex="-[ahKceASTOstZiPqbygpXEx]"
    ;;
    pmdumplog)
        all_args="adiLlmnrSsTtVvxZz"
        arg_regex="-[nSTvZ]"
    ;;
    pmdumptext)
        all_args="AaCcdFfGHhilMmNnOoPRrSstTUuVXwZz"
        arg_regex="-[AacdfhnOPRSsTtUwZ]"
    ;;
    pmevent)
        all_args="AadfghiKLnOprSsTtUVvwxZz"
        arg_regex="-[AafhiKnOpSsTtUwxZ]"
    ;;
    pminfo)
        all_args="abcdFfhKLMmNnOTtVvxZz"
        arg_regex="-[abchKNnOZ]"
    ;;
    pmlogsummary)
        all_args="aBbFfHIilMmNnpSTVvxZz"
        arg_regex="-[BnpSTZ]"
    ;;
    pmprobe)
        all_args="adfFhIiKLnOVvZz"
        arg_regex="-[ahKnOZ]"
    ;;
    pmrep)
        all_args="AabCcdEeFfGHhIiKkLlOoPpqrSsTtUuVvwXxyZz"
        arg_regex="-[AabcEeFfhiKlOoPqSsTtwXyZ]"
    ;;
    pmstore)
        all_args="fhiKLnV"
        arg_regex="-[hiKn]"
    ;;
    pmval)
        all_args="AadfghiKLnOprSsTtUvVwxZz"
        arg_regex="-[AafhiKnOpSsTtUwxZ]"
    ;;
    esac

    # Complete
    pytool=0
    [[ "pcp2elasticsearch pcp2json pcp2xlsx pcp2xml pcp2zabbix pmrep" =~ $cmd ]] && pytool=1
    if [[ "$cur" == -* ]]; then
        # Arguments
        local comp=( $(echo $all_args | sed -e 's,.\{1\},-& ,g') )
        COMPREPLY=( $(compgen -W "${comp[*]}" -- "$cur") )
    elif [[ $pytool -eq 1 && ("$cur" == :* || ${COMP_WORDS[$((COMP_CWORD-1))]} == :) ]]; then
        # pmrep(1) style metricset
        local conf=""
        for i in $(seq 1 $COMP_CWORD); do
            if [[ "${COMP_WORDS[$i]}" == -c || "${COMP_WORDS[$i]}" == --config ]]; then
                conf="${COMP_WORDS[(($i+1))]}"
                break
            fi
        done
        if [[ -z $conf ]]; then
            local sysconf=$(grep ^PCP_SYSCONF_DIR= /etc/pcp.conf 2> /dev/null | cut -d= -f2)/$cmd/$cmd.conf
            for f in ./$cmd.conf $HOME/.$cmd.conf $HOME/.pcp/$cmd.conf $sysconf; do
                [[ -f $f ]] && conf=$f && break
            done
        fi
        [[ -z $conf ]] && return
        local sets=()
        while read line; do
            if [[ $line == \[*\] && $line != \[global\] && $line != \[options\] ]]; then
                local set=${line/[} ; set=${set/]}
                sets+=($set)
            fi
        done < $conf
        [[ "$cur" == : ]] && cur=
        COMPREPLY=( $(compgen -W "${sets[*]}" -- "$cur") )
    elif [[ ! "${COMP_WORDS[$((COMP_CWORD-1))]}" =~ $arg_regex ]]; then
        # Metrics
        if [[ $cmd != pmlogsummary || \
            ( $COMP_CWORD > 1 && ${COMP_WORDS[$((COMP_CWORD-1))]} != -* && ${COMP_WORDS[$((COMP_CWORD-2))]} != -* ) ]]; then
            COMPREPLY=( $(compgen -W '$(command pminfo 2> /dev/null)' -- "$cur") )
        fi
    fi
}
complete -F _pcp_complete -o default pcp2elasticsearch pcp2json pcp2xlsx pcp2xml pcp2zabbix pmdumplog pmdumptext pmevent pminfo pmlogsummary pmprobe pmrep pmstore pmval
