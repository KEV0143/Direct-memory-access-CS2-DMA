static std::string BuildMapKeyFromBounds(const Vector3& mins, const Vector3& maxs)
{
    return radar::BuildMapKeyFromBounds(mins.x, mins.y, maxs.x, maxs.y);
}
