local M = {}

local state_by_buf = {}

function M.create(source_buf)
	return {
		source_buf = source_buf,
		job_id = nil,
		current_frame = {},
		in_frame = false,
		pending_text = nil,
		pending_fg = nil,
		stdout_fragment = "",
		stderr_fragment = "",
		stderr_lines = {},
		original_lines = nil,
		original_modified = false,
		original_modifiable = true,
		original_readonly = false,
		rendered = false,
		yaw = 0,
		pitch = 0,
		renderer_busy = false,
		pending_render = nil,
		stopping_job = false,
	}
end

function M.get(buf)
	return state_by_buf[buf]
end

function M.set(buf, value)
	state_by_buf[buf] = value
	return value
end

function M.remove(buf)
	state_by_buf[buf] = nil
end

function M.all()
	return state_by_buf
end

return M
