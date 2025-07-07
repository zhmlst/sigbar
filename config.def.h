static const char spinner[] = R"(
	spinner='\|/-'
	while :; do
	  for ((i=0; i<${#spinner}; i++)); do
		printf " %s waiting..." "${spinner:$i:1}"
		sleep 0.3
	  done
	done
)";

static const char counter[] = R"(
	echo 0
	trap '((counter++)); echo $counter' SIGRTMIN
	while :; do
	  read _
	done
)";

static const Spec specs[] = {
	/* command, signal */
	{ spinner },
	{ counter, 1 },
};

static const char delimiter[] = " | ";
