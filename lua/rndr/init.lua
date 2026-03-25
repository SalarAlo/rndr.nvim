local assets = require("rndr.core.assets")
local buffer = require("rndr.core.buffer")
local config_state = require("rndr.core.config")
local renderer = require("rndr.core.renderer")
local state_store = require("rndr.core.state")
local util = require("rndr.core.util")

local M = {}

local augroup = vim.api.nvim_create_augroup("RndrAutoOpen", { clear = true })
local is_shutting_down = false

local function cleanup_state(buf)
	local state = state_store.get(buf)
	if not state then
		return
	end

	renderer.stop_job(state)
	if not is_shutting_down then
		buffer.restore_source_buffer(state)
	end
	state_store.remove(buf)
end

local function set_buffer_keymap(buf, lhs, rhs, desc)
	if type(lhs) ~= "string" or lhs == "" then
		return
	end

	vim.keymap.set("n", lhs, rhs, {
		buffer = buf,
		silent = true,
		desc = desc,
	})
end

local function setup_buffer_keymaps(buf)
	local keymaps = (config_state.get().controls or {}).keymaps or {}
	set_buffer_keymap(buf, keymaps.rotate_left, "<Cmd>RndrRotateLeft<CR>", "Rndr rotate left")
	set_buffer_keymap(buf, keymaps.rotate_right, "<Cmd>RndrRotateRight<CR>", "Rndr rotate right")
	set_buffer_keymap(buf, keymaps.rotate_up, "<Cmd>RndrRotateUp<CR>", "Rndr rotate up")
	set_buffer_keymap(buf, keymaps.rotate_down, "<Cmd>RndrRotateDown<CR>", "Rndr rotate down")
	set_buffer_keymap(buf, keymaps.reset_view, "<Cmd>RndrResetView<CR>", "Rndr reset view")
	set_buffer_keymap(buf, keymaps.rerender, "<Cmd>RndrOpen<CR>", "Rndr rerender")
	set_buffer_keymap(buf, keymaps.close, "<Cmd>RndrClose<CR>", "Rndr restore source buffer")
end

local function ensure_state(source_buf)
	local state = state_store.get(source_buf)
	if state then
		return state
	end

	state = state_store.set(source_buf, state_store.create(source_buf))
	setup_buffer_keymaps(source_buf)

	vim.api.nvim_create_autocmd("BufWipeout", {
		group = augroup,
		buffer = source_buf,
		callback = function()
			cleanup_state(source_buf)
		end,
	})

	vim.api.nvim_create_autocmd({ "BufDelete", "BufUnload" }, {
		group = augroup,
		buffer = source_buf,
		callback = function()
			cleanup_state(source_buf)
		end,
	})

	vim.api.nvim_create_autocmd("BufHidden", {
		group = augroup,
		buffer = source_buf,
		callback = function()
			local current_state = state_store.get(source_buf)
			if current_state then
				renderer.stop_job(current_state)
			end
		end,
	})

	vim.api.nvim_create_autocmd("BufWritePre", {
		group = augroup,
		buffer = source_buf,
		callback = function()
			local current_state = state_store.get(source_buf)
			if current_state and current_state.rendered then
				buffer.restore_source_buffer(current_state)
			end
		end,
	})

	vim.api.nvim_create_autocmd("BufWritePost", {
		group = augroup,
		buffer = source_buf,
		callback = function()
			local current_state = state_store.get(source_buf)
			if current_state then
				current_state.original_lines = nil
			end
			if assets.buffer_is_renderable(source_buf, config_state.get()) then
				vim.schedule(function()
					if util.is_valid_buffer(source_buf) then
						M.open_for_buffer(source_buf)
					end
				end)
			end
		end,
	})

	return state
end

local function render_request(path, term_w, term_h, render, yaw, pitch)
	return table.concat({
		path,
		tostring(term_w),
		tostring(term_h),
		tostring(render.supersample),
		tostring(yaw),
		tostring(pitch),
		tostring(render.brightness),
		tostring(render.saturation),
		tostring(render.contrast),
		tostring(render.gamma),
		render.background,
	}, "\t") .. "\n"
end

function M.defaults()
	return config_state.defaults()
end

function M.start(path, opts)
	opts = opts or {}

	if not path or path == "" then
		util.notify("No file provided", vim.log.levels.ERROR)
		return
	end

	if vim.fn.filereadable(path) ~= 1 then
		util.notify("File is not readable: " .. path, vim.log.levels.ERROR)
		return
	end

	local current_config = config_state.get()
	local render_path, prepare_err = assets.prepare_path_for_render(path, current_config)
	if not render_path then
		util.notify(prepare_err, vim.log.levels.ERROR)
		return
	end

	local source_buf = opts.source_buf
	if not util.is_valid_buffer(source_buf) then
		source_buf = vim.api.nvim_get_current_buf()
	end

	local state = ensure_state(source_buf)
	renderer.reset_frame_state(state)
	buffer.apply_window_options(vim.fn.bufwinid(source_buf), current_config)
	vim.o.termguicolors = current_config.termguicolors

	local term_w, term_h = config_state.term_dimensions()
	local render = config_state.render_settings()
	if not renderer.ensure_renderer(state, current_config, buffer) then
		return
	end

	local request = render_request(render_path, term_w, term_h, render, state.yaw, state.pitch)
	if state.renderer_busy then
		state.pending_render = request
		return
	end

	vim.fn.chansend(state.job_id, request)
	state.renderer_busy = true
end

function M.start_current_file()
	local source_buf = vim.api.nvim_get_current_buf()
	local path = vim.api.nvim_buf_get_name(source_buf)

	if path == "" then
		util.notify("No file in current buffer", vim.log.levels.ERROR)
		return
	end

	M.start(path, { source_buf = source_buf })
end

function M.open_for_buffer(target_buf)
	target_buf = target_buf or 0
	target_buf = target_buf == 0 and vim.api.nvim_get_current_buf() or target_buf

	if not state_store.get(target_buf) and not assets.buffer_is_renderable(target_buf, config_state.get()) then
		return false
	end

	M.start(vim.api.nvim_buf_get_name(target_buf), { source_buf = target_buf })
	return true
end

local function active_state_for_current_buffer()
	local target_buf = vim.api.nvim_get_current_buf()
	local state = state_store.get(target_buf)

	if state then
		return state, target_buf
	end

	if assets.buffer_is_renderable(target_buf, config_state.get()) then
		return ensure_state(target_buf), target_buf
	end

	return nil, target_buf
end

local function rotate_current_buffer(delta_yaw, delta_pitch)
	local state, target_buf = active_state_for_current_buffer()
	if not state then
		util.notify("Current buffer is not renderable", vim.log.levels.ERROR)
		return
	end

	state.yaw = state.yaw + delta_yaw
	state.pitch = math.max(-90, math.min(90, state.pitch + delta_pitch))
	M.open_for_buffer(target_buf)
end

local function reset_current_buffer_view()
	local state, target_buf = active_state_for_current_buffer()
	if not state then
		util.notify("Current buffer is not renderable", vim.log.levels.ERROR)
		return
	end

	state.yaw = 0
	state.pitch = 0
	M.open_for_buffer(target_buf)
end

local function restore_current_buffer()
	local target_buf = vim.api.nvim_get_current_buf()
	local state = state_store.get(target_buf)
	if not state then
		return false
	end

	renderer.stop_job(state)
	buffer.restore_source_buffer(state)
	return true
end

local function rerender_visible_buffers()
	for buf, state in pairs(state_store.all()) do
		if state.rendered and util.is_valid_buffer(buf) and vim.api.nvim_buf_get_name(buf) ~= "" then
			vim.schedule(function()
				if state_store.get(buf) == state and util.is_valid_buffer(buf) then
					M.open_for_buffer(buf)
				end
			end)
		end
	end
end

local function recreate_user_command(name, callback, opts)
	pcall(vim.api.nvim_del_user_command, name)
	vim.api.nvim_create_user_command(name, callback, opts)
end

function M.health_status()
	local current_config = config_state.get()
	return {
		renderer_bin = current_config.renderer_bin,
		renderer_readable = vim.fn.filereadable(current_config.renderer_bin) == 1,
		renderer_executable = vim.fn.executable(current_config.renderer_bin) == 1,
		svg_rasterizers = assets.available_svg_rasterizers(),
	}
end

function M.setup(opts)
	local current_config = config_state.setup(opts)

	recreate_user_command("RndrOpen", function(command_opts)
		local path = command_opts.args ~= "" and command_opts.args or vim.api.nvim_buf_get_name(0)
		if command_opts.args ~= "" then
			path = vim.fn.fnamemodify(path, ":p")
		end
		M.start(path)
	end, {
		nargs = "?",
		complete = "file",
	})

	recreate_user_command("RndrClose", function()
		if not restore_current_buffer() then
			util.notify("Current buffer is not showing an rndr preview", vim.log.levels.WARN)
		end
	end, {})

	recreate_user_command("RndrRotateLeft", function()
		rotate_current_buffer(-config_state.rotate_step(), 0)
	end, {})

	recreate_user_command("RndrRotateRight", function()
		rotate_current_buffer(config_state.rotate_step(), 0)
	end, {})

	recreate_user_command("RndrRotateUp", function()
		rotate_current_buffer(0, -config_state.rotate_step())
	end, {})

	recreate_user_command("RndrRotateDown", function()
		rotate_current_buffer(0, config_state.rotate_step())
	end, {})

	recreate_user_command("RndrResetView", function()
		reset_current_buffer_view()
	end, {})

	vim.api.nvim_clear_autocmds({ group = augroup })

	vim.api.nvim_create_autocmd({ "QuitPre", "VimLeavePre" }, {
		group = augroup,
		callback = function()
			is_shutting_down = true
			for buf, state in pairs(state_store.all()) do
				if state then
					renderer.stop_job(state)
				end
				state_store.remove(buf)
			end
		end,
	})

	if current_config.render_on_resize then
		vim.api.nvim_create_autocmd({ "VimResized", "WinResized" }, {
			group = augroup,
			callback = function()
				rerender_visible_buffers()
			end,
		})
	end

	if not current_config.auto_open then
		return
	end

	for _, event in ipairs(current_config.auto_open_events or {}) do
		vim.api.nvim_create_autocmd(event, {
			group = augroup,
			callback = function(event_args)
				if util.is_valid_buffer(event_args.buf) then
					M.open_for_buffer(event_args.buf)
				end
			end,
		})
	end
end

return M
