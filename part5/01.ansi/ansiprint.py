for foreground_base in (30, 90):
    for background_base in (40, 100):
        for foreground in range(foreground_base, foreground_base + 8):
            for background in range(background_base, background_base + 8):
                color_code = f"\033[{foreground};{background}m"
                reset_code = "\033[0m"
                s = str(foreground) + ";" + str(background)
                print(f"{color_code}{s:>7}{reset_code}", end=" ")
            print()
