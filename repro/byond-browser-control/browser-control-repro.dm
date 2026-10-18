world
	name = "BYOND browser-control composition reproducer"
	status = "Native map + HudLib-style dynamic browser control hover test"
	fps = 30
	icon_size = 32
	view = "24x16"
	maxx = 40
	maxy = 28
	maxz = 1
	turf = /turf/stable_ground
	mob = /mob/reproducer

	New()
		..()
		var/current_time = time2text(world.timeofday, "hh:mm:ss")
		world.log << "[current_time] repro world max=[maxx]x[maxy]x[maxz] turf=[locate(20, 14, 1)]"

area/stable_map
	name = "Stable native map"

turf/stable_ground
	name = "stable map tile"
	icon = 'map-tile-a.png'

	New()
		..()
		if((x + y) % 2)
			icon = 'map-tile-b.png'
		maptext_width = 32
		maptext_height = 32
		maptext_x = 0
		maptext_y = 0
		var/tile_color = ((x + y) % 2) ? "#214c64" : "#285a70"
		maptext = "<font color=[tile_color]>██</font>"

obj/stable_marker
	name = "native map animation"
	layer = 10
	maptext_width = 320
	maptext_height = 48
	maptext_x = 0
	maptext_y = 0
	maptext = "<font color=#ffe89a><b>STABLE BYOND MAP</b></font>"

	New()
		..()
		spawn() animate_marker()

	proc/animate_marker()
		while(src)
			animate(src, pixel_x = 16, time = 8)
			sleep(8)
			animate(src, pixel_x = -16, time = 8)
			sleep(8)

obj/hud_trigger
	name = "native HUD browser trigger"
	layer = 20
	maptext_width = 320
	maptext_height = 64
	maptext_x = 0
	maptext_y = 0
	maptext = "<font color=#ffe89a><b>HUDLIB-STYLE BROWSER</b><br><font color=#b8d7e6>click anywhere on the native map to open the HTML surface</font></font>"

	MouseDown(atom/location, control, params)
		var/mob/reproducer/player = usr
		if(player)
			player.note_hud_event("MOUSEDOWN", control)
			player.show_hud_browser(src, params)

client
	Click(atom/object, location, control, params)
		var/mob/reproducer/player = mob
		if(player)
			player.note_hud_event("CLIENT_CLICK", control)
		// The control argument is the fully-qualified skin ID, not just the
		// element name. For this .dmf it is "reprowindow.map".
		if(player && (control == "reprowindow.map" || control == "map"))
			player.note_hud_event("CLIENT_CLICK_ACCEPTED", control)
			player.show_hud_browser(player.browser_trigger, params)
		else if(player)
			player.note_hud_event("CLIENT_CLICK_IGNORED", control)
		return ..()

mob/reproducer
	name = "composition test client"
	var/obj/hud_trigger/browser_trigger
	var/hud_trace_id = 0

	Login()
		..()
		// Give DreamSeeker a real map eye. Without a location the MAP control
		// is correctly created but has no view, which appears as a black map.
		loc = locate(20, 14, 1)
		var/obj/stable_marker/marker = new(locate(8, 14, 1))
		marker.maptext = "<font color=#ffe89a><b>STABLE BYOND MAP</b><br><font color=#b8d7e6>native map layer; it should remain visible</font></font>"
		browser_trigger = new/obj/hud_trigger(locate(14, 8, 1))
		initialize_hud_browsers()

	proc
		initialize_hud_browsers()
			// This follows HudLib Widget Pack Basic's TextInput setup:
			// create a hidden browser matching the map, then a tiny visible
			// browser beside the map. The second browser is moved and resized
			// when a HUD control needs HTML input.
			var/list/map_params = params2list(winget(src, ":map", "parent;pos;size;anchor1;anchor2"))
			if(!map_params["parent"])
				map_params["parent"] = "reprowindow"
			map_params["type"] = "browser"
			map_params["is-visible"] = "false"
			map_params["inner-background-color"] = "transparent"
			map_params["show-history"] = "false"
			map_params["show-url"] = "false"
			map_params["use-title"] = "false"
			winset(src, "mapbrowser", map_params)

			winset(src, "hudinput", list(
				"parent" = map_params["parent"],
				"type" = "browser",
				"size" = "1x1",
				"pos" = "-1,-1",
				"inner-background-color" = "transparent",
				"show-history" = "false",
				"show-url" = "false",
				"use-title" = "false",
				"is-visible" = "true"))
			var/parent_name = map_params["parent"]
			var/map_status = winget(src, "mapbrowser", "type;parent;pos;size;is-visible")
			var/input_status = winget(src, "hudinput", "type;parent;pos;size;is-visible")
			world.log << "hudlib controls created parent=[parent_name] map=[map_status] input=[input_status]"
			winset(src, "reprowindow", list("title" = "BYOND HudLib | INIT | [input_status]"))

			// Loading both documents now matches the library: the map browser
			// is the hidden positioning surface and hudinput is the transient
			// HTML surface that later appears over the native map.
			src << output(file2text('html/mapbrowser.html'), "mapbrowser")
			src << output(file2text('html/overlay.html'), "hudinput")
			trace_hud_state("INIT")

		note_hud_event(stage, detail)
			world.log << "hudlib event stage=[stage] detail=[detail]"
			winset(src, "reprowindow", list("title" = "BYOND HudLib | [stage] | [detail]"))

		trace_hud_state(stage)
			var/trace_id = ++hud_trace_id
			spawn()
				var/last_status = ""
				for(var/sample = 1, sample <= 20, sample++)
					sleep(1)
					if(trace_id != hud_trace_id)
						return
					var/status = winget(src, "hudinput", "type;parent;pos;size;is-visible;focus")
					world.log << "hudlib state stage=[stage] sample=[sample] input=[status]"
					if(sample == 1 || status != last_status)
						winset(src, "reprowindow", list("title" = "BYOND HudLib | [stage] | [status]"))
						last_status = status

		show_hud_browser(atom/target, click_params)
			if(!target)
				target = browser_trigger
			if(!target || !client)
				return
			var/screen_position = "12:1,8:1"
			if(click_params)
				var/list/click_data = params2list(click_params)
				if(click_data["screen-loc"])
					screen_position = click_data["screen-loc"]
			// MAP_BROWSER_HTML expects the complete screen_loc form
			// "tile_x:pixel_x,tile_y:pixel_y". Passing only "x,y" makes
			// screenloc() produce NaN coordinates, so hudinput remains at its
			// 1x1 staging position and the HTML page is never visible.
			if(!findtext(screen_position, ":") || !findtext(screen_position, ","))
				screen_position = "12:1,8:1"
			var/list/payload = list(
				screen_position,
				"1x1",
				"400x330",
				client.view,
				world.icon_size,
				"\ref[target]",
				"",
				"")
			src << output(list2params(payload), "mapbrowser:onTextInput")
			world.log << "hudlib show requested target=[target.x],[target.y] screen=[screen_position]"
			note_hud_event("HANDOFF_SENT", screen_position)

		hide_hud_browser()
			winset(src, "hudinput", list(
				"is-visible" = "false",
				"pos" = "-1,-1",
				"size" = "1x1"))
			note_hud_event("HIDE_REQUESTED", "hudinput")

	verb/reload_html()
		set hidden = 1
		initialize_hud_browsers()
		show_hud_browser(browser_trigger)

	verb/show_html_hud()
		show_hud_browser(browser_trigger)

	verb/hide_html_hud()
		hide_hud_browser()

	verb/repro_help()
		src << "<b>Reproducer instructions:</b><br>Confirm the native map remains visible. Click the HUDLIB-STYLE BROWSER marker, or use Show HTML HUD, to send the mapbrowser JavaScript handoff. Then move the pointer across the HTML controls repeatedly. Use Hide HTML HUD and Show HTML HUD to exercise the runtime browser lifecycle. The HTML panel must not disappear or flicker."
