static const char spinner[] = R"(#!/bin/sh
spinner='\|/-'
while :; do
  for ((i=0; i<${#spinner}; i++)); do
    printf " %s waiting..." "${spinner:$i:1}"
    sleep 0.3
  done
done
)";

static const char counter[] = R"(#!/bin/sh
counter=0
echo $counter
while read _; do
  ((counter++))
  echo $counter
done
)";

static const Spec specs[] = {
	/* command, signal */
	{ spinner },
	{ counter, 1 },
};

static const char delimiter[] = " | ";
