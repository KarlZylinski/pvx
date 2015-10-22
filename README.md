# pvx
This library can be used to draw simple, colored shapes from lua. It is a minimalistic C lib. It was made for my game "KOFFERT", which is available here: https://github.com/KarlZylinski/town

You load it like so:
```package.loadlib("pvx.dll", "pvx_load")()```

This injects these global functions into your lua environment, which you can then use to draw graphics and process simple input:

```pvx_init
pvx_deinit
pvx_process_events
pvx_is_window_open
pvx_add_shape
pvx_draw_shape
pvx_clear
pvx_flip
pvx_key_held
pvx_move_view
pvx_view_pos
pvx_mouse_pos
pvx_window_size
pvx_left_mouse_held
pvx_right_mouse_held```
