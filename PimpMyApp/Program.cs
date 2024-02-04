// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Runtime.CompilerServices;

namespace PimpMyApp;

public class Program
{
    public static int Main()
    {
        int value = Bambala(23, 45);
        Console.ReadKey(true);
        return value;
    }

    [MethodImpl(MethodImplOptions.NoInlining)]
    private static int Bambala(int x, int y) => (x | 5) | (y | 3);
}
