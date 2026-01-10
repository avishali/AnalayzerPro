# AxisRenderer usage inventory (should only be the already-migrated files)
grep -R --line-number "mdsp_ui::AxisRenderer" Source/ui | sed -n '1,200p'

# AxisInteraction usage inventory (should only be intentional hover/readout files)
grep -R --line-number "AxisInteraction" Source/ui | sed -n '1,200p'

# Heuristic: manual axis-like tick/label loops in view code (review any hits)
grep -R --line-number --extended-regexp \
"tick(s)?|Axis|Hz|kHz|dB|Justification::centred(Top|Right)?|drawText|drawLine" \
Source/ui/views Source/ui/analyzer | sed -n '1,200p'