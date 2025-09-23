#include <echion/frame.h>
#include <echion/render.h>

// ------------------------------------------------------------------------
Result<void> WhereRenderer::render_frame_internal(Frame& frame)
{
    auto name_result = string_table.lookup(frame.name);
    if (!name_result)
        return Result<void>::error(ErrorKind::LookupError);

    auto filename_result = string_table.lookup(frame.filename);
    if (!filename_result)
        return Result<void>::error(ErrorKind::LookupError);

    auto line = frame.location.line;

    const std::string& name_str = **name_result;
    const std::string& filename_str = **filename_result;

    if (filename_str.rfind("native@", 0) == 0)
    {
        WhereRenderer::get().render_message(
            "\033[38;5;248;1m" + name_str + "\033[0m \033[38;5;246m(" + filename_str +
            "\033[0m:\033[38;5;246m" + std::to_string(line) + ")\033[0m");
    }
    else
    {
        WhereRenderer::get().render_message("\033[33;1m" + name_str + "\033[0m (\033[36m" +
                                            filename_str + "\033[0m:\033[32m" +
                                            std::to_string(line) + "\033[0m)");
    }

    return Result<void>::ok();
}

void WhereRenderer::render_frame(Frame& frame)
{
    render_frame_internal(frame);
}

// ------------------------------------------------------------------------
void MojoRenderer::render_frame(Frame& frame)
{
    frame_ref(frame.cache_key);
}
