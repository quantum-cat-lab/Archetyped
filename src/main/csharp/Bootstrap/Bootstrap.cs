using System;
using System.Collections.Generic;
using System.Linq;
using System.Reflection;
using System.Runtime.InteropServices;
using System.Runtime.Loader;

class DomainALC : AssemblyLoadContext
{
    string domainName;
    public DomainALC(string name) : base(name, isCollectible: true) { domainName = name; }
    protected override Assembly? Load(AssemblyName name)
    {
        if (Bootstrap.TryResolveImport(domainName, name, out var asm)) return asm;
        return null;
    }
}

public static class Bootstrap
{
    static Dictionary<string, DomainALC> alcs = new();
    static Dictionary<string, Assembly> assemblies = new();
    static Dictionary<string, List<string>> imports = new(); // domain -> [importDomain]

    internal static bool TryResolveImport(string domain, AssemblyName name, out Assembly? asm)
    {
        asm = null;
        if (!imports.TryGetValue(domain, out var list)) return false;
        foreach (var imp in list)
        {
            if (assemblies.TryGetValue(imp, out var a) && a.GetName().Name == name.Name) { asm = a; return true; }
            if (alcs.TryGetValue(imp, out var alc))
            {
                foreach (var loaded in alc.Assemblies)
                    if (loaded.GetName().Name == name.Name) { asm = loaded; return true; }
            }
        }
        return false;
    }

    [UnmanagedCallersOnly(EntryPoint = "CreateALC")]
    public static int CreateALC(nint domainPtr)
    {
        var domain = Marshal.PtrToStringUTF8(domainPtr);
        if (string.IsNullOrEmpty(domain)) return -1;
        if (alcs.ContainsKey(domain)) return 0;
        alcs[domain] = new DomainALC(domain);
        Console.WriteLine($"[ManagedBootstrap] CreateALC '{domain}'");
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "LoadAssembly")]
    public static int LoadAssembly(nint domainPtr, nint pathPtr)
    {
        var domain = Marshal.PtrToStringUTF8(domainPtr);
        var path = Marshal.PtrToStringUTF8(pathPtr);
        if (domain == null || path == null) return -1;
        if (!alcs.TryGetValue(domain, out var alc)) return -2;
        try
        {
            var asm = alc.LoadFromAssemblyPath(path);
            assemblies[domain] = asm;
            Console.WriteLine($"[ManagedBootstrap] LoadAssembly '{domain}' -> {path} : {asm.FullName}");
            return 0;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ManagedBootstrap] LoadAssembly failed {path}: {ex}");
            return -3;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "GetFunctionPointer")]
    public static nint GetFunctionPointer(nint domainPtr, nint typePtr, nint methodPtr)
    {
        var domain = Marshal.PtrToStringUTF8(domainPtr);
        var typeName = Marshal.PtrToStringUTF8(typePtr);
        var methodName = Marshal.PtrToStringUTF8(methodPtr);
        if (domain == null || typeName == null || methodName == null) return 0;
        if (!assemblies.TryGetValue(domain, out var asm))
        {
            if (!alcs.TryGetValue(domain, out var alc)) return 0;
            asm = alc.Assemblies.FirstOrDefault();
            if (asm == null) return 0;
        }
        var type = asm.GetType(typeName);
        if (type == null) { Console.WriteLine($"[ManagedBootstrap] GetFunc type not found {typeName}"); return 0; }
        var method = type.GetMethod(methodName, BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Static);
        if (method == null) { Console.WriteLine($"[ManagedBootstrap] GetFunc method not found {typeName}.{methodName}"); return 0; }
        try
        {
            var ptr = method.MethodHandle.GetFunctionPointer();
            Console.WriteLine($"[ManagedBootstrap] GetFunctionPointer {typeName}.{methodName} -> 0x{ptr:X}");
            return ptr;
        }
        catch (Exception ex)
        {
            Console.WriteLine($"[ManagedBootstrap] GetFunctionPointer failed: {ex}");
            return 0;
        }
    }

    [UnmanagedCallersOnly(EntryPoint = "AddImport")]
    public static int AddImport(nint domainPtr, nint importPtr)
    {
        var domain = Marshal.PtrToStringUTF8(domainPtr);
        var importDomain = Marshal.PtrToStringUTF8(importPtr);
        if (domain == null || importDomain == null) return -1;
        if (!imports.TryGetValue(domain, out var list)) { list = new List<string>(); imports[domain] = list; }
        if (!list.Contains(importDomain)) list.Add(importDomain);
        Console.WriteLine($"[ManagedBootstrap] AddImport '{domain}' -> '{importDomain}'");
        return 0;
    }

    [UnmanagedCallersOnly(EntryPoint = "UnloadALC")]
    public static int UnloadALC(nint domainPtr)
    {
        var domain = Marshal.PtrToStringUTF8(domainPtr);
        if (domain == null) return -1;
        assemblies.Remove(domain);
        imports.Remove(domain);
        foreach (var kv in imports) kv.Value.Remove(domain);
        if (alcs.TryGetValue(domain, out var alc))
        {
            alc.Unload();
            alcs.Remove(domain);
            GC.Collect();
            GC.WaitForPendingFinalizers();
            GC.Collect();
            Console.WriteLine($"[ManagedBootstrap] UnloadALC '{domain}'");
            return 0;
        }
        return -2;
    }
}
