inline int clamp(int i, int min, int max)
{
    const int t = i < min ? min : i;
    return t > max ? max : t;
}

inline int map(int x, int in_min, int in_max, int out_min, int out_max)
{
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}