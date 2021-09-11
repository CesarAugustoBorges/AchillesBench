# ---------------------------------------------------------------------------- #

# TODO: port to other shells

_bdus_completion()
{
    local -r devs="$(find /dev -maxdepth 1 -type b -name 'bdus-?*' | sort -V)"
    local candidates=''

    case "${#COMP_WORDS[@]}" in

        2)
            if [[ "${COMP_WORDS[COMP_CWORD]}" = -* ]]; then
                candidates='--help --version'
            else
                candidates='destroy'
            fi
            ;;

        3)
            if [[ "${COMP_WORDS[COMP_CWORD - 1]}" = destroy ]]; then
                if [[ "${COMP_WORDS[COMP_CWORD]}" = -* ]]; then
                    candidates='--help --no-flush'
                else
                    candidates="${devs}"
                fi
            fi
            ;;

        4)
            if [[ "${COMP_WORDS[COMP_CWORD - 2]}" = destroy ]]; then
                if [[ "${COMP_WORDS[COMP_CWORD - 1]}" = --no-flush ]]; then
                    candidates="${devs}"
                elif [[ "${COMP_WORDS[COMP_CWORD]}" = -* ]]; then
                    candidates='--no-flush'
                fi
            fi
            ;;

    esac

    COMPREPLY=($(compgen -W "${candidates}" -- "${COMP_WORDS[COMP_CWORD]}"))
}

complete -F _bdus_completion bdus

# ---------------------------------------------------------------------------- #
