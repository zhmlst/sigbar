static const char keymap[] = R"(#!/usr/bin/env bash
print_status(){
  sed -e 's/English (US)/ EN/g' -e 's/Russian/ RU/g' /tmp/dwl-keymap
}
print_status
while read _; do print_status; done
)";


static const char volume[] = R"(#!/usr/bin/env bash
print_volume() {
  out=$(wpctl get-volume @DEFAULT_AUDIO_SINK@)
  vol=$(awk '{print $2}' <<< "$out")
  is_muted=$(grep -q MUTED <<< "$out" && echo 1 || echo 0)

  if (( is_muted )); then
    icon="󰝟"
    perc=$(awk -v v="$vol" 'BEGIN { printf "%.0f", v * 100 }')
  else
    perc=$(awk -v v="$vol" 'BEGIN { printf "%.0f", v * 100 }')
    if (( perc <= 30 )); then
      icon="󰕿"
    elif (( perc <= 70 )); then
      icon="󰖀"
    else
      icon="󰕾"
    fi
  fi

  echo "$icon $perc%"
}

print_volume
while read _; do print_volume; done
)";

static const char bluetooth[] = R"(#!/usr/bin/env bash
print_bluetooth() {
  if ! bluetoothctl show | grep -q "Powered: yes"; then
    echo "󰂲 0"
    return
  fi

  local count=0
  for dev in $(bluetoothctl devices | awk '{print $2}'); do
    if bluetoothctl info "$dev" | grep -q "Connected: yes"; then
      ((count++))
    fi
  done

  local icon
  if (( count == 0 )); then
    icon="󰂯"
  else
    icon="󰂱"
  fi

  echo "$icon $count"
}

print_bluetooth
bluetoothctl -m | while read -r _; do print_bluetooth; done
)";

static const char network[] = R"(#!/usr/bin/env bash
print_connection(){
  nmcli -f NAME c | sed -n 2p | tr -d ' '
}

print_connection
nmcli monitor | while read -r line; do
  if [[ "$line" == *"primary connection"* ]]; then
    print_connection
  fi
done
)";

static const char battery[] = R"(#!/usr/bin/env bash
discharging_icons=(󰂎 󰁺 󰁻 󰁼 󰁽 󰁾 󰁿 󰂀 󰂁 󰂂 󰁹)
charging_icons=(󰢟 󰢜 󰂆 󰂇 󰂈 󰢝 󰂉 󰢞 󰂊 󰂋 󰂅)

batteries=$(upower -e | grep battery)

print_power() {
  for bat in $batteries; do
    read -r state pct < <(
      upower -i "$bat" | awk '/state:/{s=$2} /percentage:/{print s, $2}'
    )
    pct_num=${pct%\%}
    idx=$(( pct_num / 10 ))
    (( idx > 10 )) && idx=10

    if [[ $state == "charging" ]]; then
      icon=${charging_icons[idx]}
    else
      icon=${discharging_icons[idx]}
    fi

    printf "%s %s\n" "$icon" "$pct"
  done
}

print_power
upower -m | while read _; do print_power; done
)";

static const Spec specs[] = {
	/* command, signal */
	{ keymap, 1 },
	{ volume, 2 },
	{ bluetooth, 3 },
	{ network, 4 },
	{ battery, 5 },
	{ "while :; do date '+%m.%d %H:%M' && sleep 60; done" },
};

static const char delimiter[] = " | ";
