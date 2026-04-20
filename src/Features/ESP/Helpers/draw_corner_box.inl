static void DrawCornerBox(ImDrawList* dl, float x, float y, float w, float h,
                          ImU32 color, ImU32 outline, float cornerLen, float thickness)
{
    float cl = cornerLen;
    if (cl > w * 0.5f) cl = w * 0.5f;
    if (cl > h * 0.5f) cl = h * 0.5f;

    float ot = thickness + 2.0f;

    dl->AddLine(ImVec2(x - 1, y), ImVec2(x + cl, y), outline, ot);
    dl->AddLine(ImVec2(x, y - 1), ImVec2(x, y + cl), outline, ot);
    dl->AddLine(ImVec2(x + w - cl, y), ImVec2(x + w + 1, y), outline, ot);
    dl->AddLine(ImVec2(x + w, y - 1), ImVec2(x + w, y + cl), outline, ot);
    dl->AddLine(ImVec2(x - 1, y + h), ImVec2(x + cl, y + h), outline, ot);
    dl->AddLine(ImVec2(x, y + h - cl), ImVec2(x, y + h + 1), outline, ot);
    dl->AddLine(ImVec2(x + w - cl, y + h), ImVec2(x + w + 1, y + h), outline, ot);
    dl->AddLine(ImVec2(x + w, y + h - cl), ImVec2(x + w, y + h + 1), outline, ot);

    dl->AddLine(ImVec2(x, y), ImVec2(x + cl, y), color, thickness);
    dl->AddLine(ImVec2(x, y), ImVec2(x, y + cl), color, thickness);
    dl->AddLine(ImVec2(x + w - cl, y), ImVec2(x + w, y), color, thickness);
    dl->AddLine(ImVec2(x + w, y), ImVec2(x + w, y + cl), color, thickness);
    dl->AddLine(ImVec2(x, y + h), ImVec2(x + cl, y + h), color, thickness);
    dl->AddLine(ImVec2(x, y + h - cl), ImVec2(x, y + h), color, thickness);
    dl->AddLine(ImVec2(x + w - cl, y + h), ImVec2(x + w, y + h), color, thickness);
    dl->AddLine(ImVec2(x + w, y + h - cl), ImVec2(x + w, y + h), color, thickness);
}
