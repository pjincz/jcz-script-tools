DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"

export PATH="$PATH:$DIR"

function _new_completion()
{
	local cur=${COMP_WORDS[COMP_CWORD]}
	COMPREPLY=( $(compgen -W "$(ls $DIR/template | sed 's/\.sh//')" -- $cur) )
}

complete -F _new_completion new
