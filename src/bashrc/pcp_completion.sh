# Programmable completion for Performance Co-Pilot commands under bash.
_pcp_complete()
{
    local cur=${COMP_WORDS[$COMP_CWORD]}
    local curpos_expand=0
    local opt_regex=""

    COMPREPLY=()

    # Options that need no completion and the cursor position to start
    # expansion from for different programs
    case ${COMP_WORDS[0]} in

        pmdumplog)
        opt_regex="-[adiLlmnrSsTtVvxZz]"
        curpos_expand=1
        ;;

        pmdumptext)
        opt_regex="-[AaCcdFfGHhilMmNnOoPRrSstTUuVXwZz]"
        curpos_expand=1
        ;;

        pmevent)
        opt_regex="-[AadfghiKLnOprSsTtUVvwxZz]"
        curpos_expand=1
        ;;

        pminfo)
        opt_regex="-[abcdFfhKLMmNnOTtVvxZz]"
        curpos_expand=1
        ;;

        pmlogsummary)
        opt_regex="-[aBbFfHIilMmNnpSTVvxZz]"
        curpos_expand=2
        ;;

        pmprobe)
        opt_regex="-[adfFhIiKLnOVvZz]"
        curpos_expand=1
        ;;

        pmrep)
        opt_regex="-[AabCcdEeFfGHhIiKLlOoPpqrSsTtUuVvwXxyZz]"
        curpos_expand=1
        ;;

        pmstore)
        opt_regex="-[fhiKLnV]"
        curpos_expand=1
        ;;

        pmval)
        opt_regex="-[AadfghiKLnOprSsTtUvVwxZz]"
        curpos_expand=1
        ;;

    esac    # --- end of case ---

    # We expand either straight from the cursor if it is at the position to
    # expand or check for the preceding options whether to expand or not
    if (( $COMP_CWORD == $curpos_expand )) || \
        ( (( $COMP_CWORD > $curpos_expand )) \
            && ! [[ "${COMP_WORDS[$((COMP_CWORD-1))]}" =~ $opt_regex ]]
        )
    then
        COMPREPLY=(`compgen -W '$(command pminfo)' 2>/dev/null $cur`)
    fi

}    # ----------  end of function _pcp_complete  ----------

complete -F _pcp_complete -o default pmdumplog pmdumptext pmevent pminfo pmlogsummary pmprobe pmrep pmstore pmval
