local util = require("rndr.core.util")

local M = {}

function M.reset_frame_state(state)
	state.current_frame = {}
	state.in_frame = false
	state.pending_text = nil
	state.pending_fg = nil
	state.stdout_fragment = ""
	state.stderr_fragment = ""
	state.stderr_lines = {}
end

function M.stop_job(state)
	if state and state.job_id then
		state.stopping_job = true
		vim.fn.jobstop(state.job_id)
		state.job_id = nil
	end

	if state then
		state.renderer_busy = false
		state.pending_render = nil
	end
end

local function flush_renderer_errors(state)
	if #state.stderr_lines == 0 then
		return
	end

	local message = table.concat(state.stderr_lines, "\n")
	state.stderr_lines = {}
	util.notify(message, vim.log.levels.ERROR)
end

local function handle_line(state, line, buffer_ops)
	if line == "<FRAME>" then
		state.current_frame = {}
		state.in_frame = true
		state.pending_text = nil
		state.pending_fg = nil
		return
	end

	if line == "<END>" then
		state.in_frame = false
		state.pending_text = nil
		state.pending_fg = nil
		buffer_ops.render_frame(state, state.current_frame)
		return
	end

	if line == "<DONE>" then
		state.renderer_busy = false
		flush_renderer_errors(state)
		if state.pending_render and state.job_id then
			local pending = state.pending_render
			state.pending_render = nil
			vim.fn.chansend(state.job_id, pending)
			state.renderer_busy = true
		end
		return
	end

	if not state.in_frame then
		return
	end

	if vim.startswith(line, "<TEXT>") then
		state.pending_text = line:sub(7)
		state.pending_fg = nil
		return
	end

	if vim.startswith(line, "<FG>") and state.pending_text ~= nil then
		state.pending_fg = line:sub(5)
		return
	end

	if vim.startswith(line, "<BG>") and state.pending_text ~= nil and state.pending_fg ~= nil then
		table.insert(state.current_frame, {
			text = state.pending_text,
			fg = state.pending_fg,
			bg = line:sub(5),
		})
		state.pending_text = nil
		state.pending_fg = nil
	end
end

local function process_stream(data, fragment, line_handler)
	if not data or #data == 0 then
		return fragment
	end

	fragment = fragment .. (data[1] or "")
	for index = 2, #data do
		line_handler(fragment)
		fragment = data[index] or ""
	end

	return fragment
end

local function handle_stdout_data(state, data, buffer_ops)
	state.stdout_fragment = process_stream(data, state.stdout_fragment, function(line)
		handle_line(state, line, buffer_ops)
	end)
end

local function handle_stderr_data(state, data)
	state.stderr_fragment = process_stream(data, state.stderr_fragment, function(line)
		if line ~= "" then
			table.insert(state.stderr_lines, line)
		end
	end)
end

function M.ensure_renderer(state, config, buffer_ops)
	if state.job_id and state.job_id > 0 then
		return true
	end

	M.reset_frame_state(state)

	state.job_id = vim.fn.jobstart({
		config.renderer_bin,
		"--stdio",
	}, {
		stdout_buffered = false,
		stderr_buffered = false,

		on_stdout = function(_, data)
			handle_stdout_data(state, data, buffer_ops)
		end,

		on_stderr = function(_, data)
			handle_stderr_data(state, data)
		end,

		on_exit = function(_, exit_code)
			local intentional_stop = state.stopping_job
			state.stopping_job = false
			state.job_id = nil
			state.renderer_busy = false
			state.pending_render = nil

			if state.stdout_fragment ~= "" then
				handle_line(state, state.stdout_fragment, buffer_ops)
				state.stdout_fragment = ""
			end

			if state.stderr_fragment ~= "" then
				table.insert(state.stderr_lines, state.stderr_fragment)
				state.stderr_fragment = ""
			end

			if exit_code ~= 0 and not intentional_stop then
				flush_renderer_errors(state)
				util.notify("Renderer exited with code " .. exit_code, vim.log.levels.ERROR)
			end
		end,
	})

	if state.job_id <= 0 then
		state.job_id = nil
		util.notify("Failed to start renderer: " .. config.renderer_bin, vim.log.levels.ERROR)
		return false
	end

	return true
end

return M
