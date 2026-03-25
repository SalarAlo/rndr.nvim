local util = require("rndr.core.util")

local M = {}

local legacy_defaults = {
	auto_open = false,
	auto_open_events = { "BufReadPost" },
	image_extensions = { "png", "jpg", "jpeg", "gif", "bmp", "webp", "tga", "psd", "hdr", "pic", "pnm", "ppm", "pgm", "pbm" },
	vector_extensions = { "svg", "svgz" },
	model_extensions = { "obj", "fbx", "glb", "gltf", "dae", "3ds", "blend", "ply", "stl", "x", "off" },
	renderer_bin = util.plugin_root() .. "/renderer/build/rndr",
	termguicolors = true,
	render_on_resize = true,
	win_options = {
		cursorline = false,
		wrap = false,
		list = false,
		signcolumn = "no",
		number = false,
		relativenumber = false,
		spell = false,
	},
	size = {
		width_offset = 0,
		height_offset = 0,
		min_width = 1,
		min_height = 1,
	},
	render = {
		supersample = 2,
		brightness = 1.0,
		saturation = 1.18,
		contrast = 1.08,
		gamma = 0.92,
		background = "0d0f14",
	},
	controls = {
		rotate_step = 15,
		keymaps = {
			rotate_left = nil,
			rotate_right = nil,
			rotate_up = nil,
			rotate_down = nil,
			reset_view = nil,
			rerender = nil,
			close = nil,
		},
	},
}

local defaults = {
	preview = {
		auto_open = legacy_defaults.auto_open,
		events = vim.deepcopy(legacy_defaults.auto_open_events),
		render_on_resize = legacy_defaults.render_on_resize,
	},
	assets = {
		images = vim.deepcopy(legacy_defaults.image_extensions),
		vectors = vim.deepcopy(legacy_defaults.vector_extensions),
		models = vim.deepcopy(legacy_defaults.model_extensions),
	},
	window = {
		termguicolors = legacy_defaults.termguicolors,
		size = vim.deepcopy(legacy_defaults.size),
		options = vim.deepcopy(legacy_defaults.win_options),
	},
	renderer = vim.tbl_deep_extend("force", vim.deepcopy(legacy_defaults.render), {
		bin = legacy_defaults.renderer_bin,
	}),
	controls = vim.deepcopy(legacy_defaults.controls),
}

local current = {}

local function normalize_options(opts)
	opts = opts or {}

	local normalized = vim.tbl_deep_extend("force", vim.deepcopy(defaults), opts)
	normalized.preview.auto_open = opts.auto_open ~= nil and opts.auto_open or normalized.preview.auto_open
	normalized.preview.events = opts.auto_open_events or normalized.preview.events
	normalized.preview.render_on_resize = opts.render_on_resize ~= nil and opts.render_on_resize
		or normalized.preview.render_on_resize

	normalized.assets.images = opts.image_extensions or normalized.assets.images
	normalized.assets.vectors = opts.vector_extensions or normalized.assets.vectors
	normalized.assets.models = opts.model_extensions or normalized.assets.models

	normalized.window.termguicolors = opts.termguicolors ~= nil and opts.termguicolors or normalized.window.termguicolors
	normalized.window.options = vim.tbl_deep_extend("force", vim.deepcopy(defaults.window.options), normalized.window.options or {}, opts.win_options or {})
	normalized.window.size = vim.tbl_deep_extend("force", vim.deepcopy(defaults.window.size), normalized.window.size or {}, opts.size or {})

	normalized.renderer.bin = opts.renderer_bin or normalized.renderer.bin
	normalized.controls = vim.tbl_deep_extend("force", vim.deepcopy(defaults.controls), opts.controls or {}, normalized.controls or {})

	normalized.auto_open = normalized.preview.auto_open
	normalized.auto_open_events = normalized.preview.events
	normalized.render_on_resize = normalized.preview.render_on_resize
	normalized.image_extensions = normalized.assets.images
	normalized.vector_extensions = normalized.assets.vectors
	normalized.model_extensions = normalized.assets.models
	normalized.termguicolors = normalized.window.termguicolors
	normalized.win_options = normalized.window.options
	normalized.size = normalized.window.size
	normalized.render = {
		supersample = normalized.renderer.supersample,
		brightness = normalized.renderer.brightness,
		saturation = normalized.renderer.saturation,
		contrast = normalized.renderer.contrast,
		gamma = normalized.renderer.gamma,
		background = normalized.renderer.background,
	}
	normalized.renderer_bin = normalized.renderer.bin

	return normalized
end

current = normalize_options()

function M.defaults()
	return vim.deepcopy(defaults)
end

function M.get()
	return current
end

function M.setup(opts)
	current = normalize_options(opts)
	return current
end

function M.render_settings()
	local render = current.renderer or {}
	local renderer_defaults = defaults.renderer
	local supersample = math.max(1, math.floor(tonumber(render.supersample) or 1))
	local brightness = math.max(0, tonumber(render.brightness) or renderer_defaults.brightness)
	local saturation = math.max(0, tonumber(render.saturation) or renderer_defaults.saturation)
	local contrast = math.max(0, tonumber(render.contrast) or renderer_defaults.contrast)
	local gamma = math.max(0.01, tonumber(render.gamma) or renderer_defaults.gamma)
	local background = type(render.background) == "string" and render.background:lower() or renderer_defaults.background

	if not util.is_hex_color(background) or #background ~= 6 then
		background = renderer_defaults.background
	end

	return {
		supersample = supersample,
		brightness = brightness,
		saturation = saturation,
		contrast = contrast,
		gamma = gamma,
		background = background,
	}
end

function M.term_dimensions()
	local size = (current.window or {}).size or {}
	local min_width = math.max(1, size.min_width or 1)
	local min_height = math.max(1, size.min_height or 1)
	local width_offset = size.width_offset or 0
	local height_offset = size.height_offset or 0

	local term_w = math.max(min_width, vim.api.nvim_win_get_width(0) + width_offset)
	local term_h = math.max(min_height, vim.api.nvim_win_get_height(0) + height_offset)

	return term_w, term_h
end

function M.rotate_step()
	return math.abs(tonumber((current.controls or {}).rotate_step) or 15)
end

return M
