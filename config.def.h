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
	{ counter, 1 },
	{ "while :; do date '+%m.%d %H:%M' && sleep 60; done" },
};

static const char delimiter[] = " | ";
