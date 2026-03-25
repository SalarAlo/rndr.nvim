local M = {}

local health = vim.health or require("health")

local function report_start(message)
	if health.start then
		health.start(message)
	else
		health.report_start(message)
	end
end

local function report_ok(message)
	if health.ok then
		health.ok(message)
	else
		health.report_ok(message)
	end
end

local function report_warn(message)
	if health.warn then
		health.warn(message)
	else
		health.report_warn(message)
	end
end

local function report_error(message)
	if health.error then
		health.error(message)
	else
		health.report_error(message)
	end
end

function M.check()
	local status = require("rndr").health_status()

	report_start("rndr.nvim")

	if status.renderer_readable and status.renderer_executable then
		report_ok("Renderer binary is available at " .. status.renderer_bin)
	elseif status.renderer_readable then
		report_error("Renderer binary exists but is not executable: " .. status.renderer_bin)
	else
		report_error("Renderer binary is missing: " .. status.renderer_bin .. ". Build it with `make` or `./scripts/build_renderer.sh`.")
	end

	if #status.svg_rasterizers > 0 then
		report_ok("SVG rasterizer available: " .. table.concat(status.svg_rasterizers, ", "))
	else
		report_warn("No SVG rasterizer found. SVG support needs one of: rsvg-convert, magick, convert.")
	end
end

return M
