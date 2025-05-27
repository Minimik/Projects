##### startup script #####

#!/opt/bin/lv_micropython -i

import lvgl as lv

import display_driver


##### main script #####

#!//opt/bin/lv_micropython -i
import utime as time
import lvgl as lv
import display_driver




#
# A simple meter
#
meter = lv.meter(lv.scr_act())
meter.center()
meter.set_size(200, 200)

def set_value(indic, v):
    meter.set_indicator_value(indic, v )

# Add a scale first
scale = meter.add_scale()
meter.set_scale_ticks(scale, 71, 2, 10, lv.palette_main(lv.PALETTE.GREY))
meter.set_scale_major_ticks(scale, 10, 4, 15, lv.color_black(), 20)
meter.set_scale_range(scale, 0, 70, 270, 135)
#scale.set_style_bg_color( lv.palette_main( lv.PALETTE.BLACK ) )
indic = lv.meter_indicator_t()

# Add a blue arc to the start
#indic = meter.add_arc(scale, 3, lv.palette_main(lv.PALETTE.BLUE), 0)
#meter.set_indicator_start_value(indic, 0)
#meter.set_indicator_end_value(indic, 0)

# Make the tick lines blue at the start of the scale
#indic = meter.add_scale_lines(scale, lv.palette_main(lv.PALETTE.BLUE), lv.palette_main(lv.PALETTE.BLUE), False, 0)
#meter.set_indicator_start_value(indic, 0)
#meter.set_indicator_end_value(indic, 0)

# Add a red arc to the end
indic = meter.add_arc(scale, 3, lv.palette_main(lv.PALETTE.ORANGE), 0)
meter.set_indicator_start_value(indic, 50)
meter.set_indicator_end_value(indic, 60)

# Make the tick lines red at the end of the scale
indic = meter.add_scale_lines(scale, lv.palette_main(lv.PALETTE.RED), lv.palette_main(lv.PALETTE.ORANGE), False, 0)
meter.set_indicator_start_value(indic, 50)
meter.set_indicator_end_value(indic, 60)

# Add a red arc to the end
indic = meter.add_arc(scale, 3, lv.palette_main(lv.PALETTE.RED), 0)
meter.set_indicator_start_value(indic, 60)
meter.set_indicator_end_value(indic, 70)

# Make the tick lines red at the end of the scale
indic = meter.add_scale_lines(scale, lv.palette_main(lv.PALETTE.RED), lv.palette_main(lv.PALETTE.RED), False, 0)
meter.set_indicator_start_value(indic, 60)
meter.set_indicator_end_value(indic, 70)

# Add a needle line indicator
indic = meter.add_needle_line(scale, 4, lv.palette_main(lv.PALETTE.GREY), -10)

# Create an animation to set the value
a = lv.anim_t()
a.init()
a.set_var(indic)
a.set_values(0, 70)
a.set_time(1000)
a.set_repeat_delay(100)
a.set_playback_time(250)
a.set_playback_delay(100)
a.set_repeat_count(lv.ANIM_REPEAT.INFINITE)
a.set_custom_exec_cb(lambda a, val: set_value(indic, val))
lv.anim_t.start(a)


label1 = lv.label(lv.scr_act())
#label1.set_long_mode(lv.label.LONG.WRAP)      # Break the long lines*/
label1.set_recolor(True)                      # Enable re-coloring by commands in the text
label1.set_text("#0000ff x100 RPM")
label1.set_width(100)                         # Set smaller width to make the lines wrap
label1.set_style_text_align(lv.ALIGN.CENTER, 0)
label1.align(lv.ALIGN.CENTER, +15, +55)

