// Copyright (c) .NET Foundation and contributors. All rights reserved.
// Licensed under the MIT license. See LICENSE file in the project root for full license information.

using System;
using System.Diagnostics.CodeAnalysis;
using System.Reflection;
using Mono.Linker.Tests.Cases.Expectations.Assertions;
using Mono.Linker.Tests.Cases.Expectations.Helpers;
using Mono.Linker.Tests.Cases.Expectations.Metadata;

namespace Mono.Linker.Tests.Cases.DataFlow
{
    [SkipKeptItemsValidation]
    [SandboxDependency("Dependencies/TestSystemTypeBase.cs")]
    [ExpectedNoWarnings]
    public class TypeBaseTypeDataFlow
    {
        public static void Main()
        {
            TestAllPropagated(typeof(TestType));
            AllPropagatedWithDerivedClass.Test();

            TestPublicConstructorsAreNotPropagated(typeof(TestType));
            TestPublicConstructorsWithInheritedArePropagated(typeof(TestType));
            TestPublicEventsPropagated(typeof(TestType));
            TestPublicFieldsPropagated(typeof(TestType));
            TestPublicMethodsPropagated(typeof(TestType));
            TestPublicNestedTypesAreNotPropagated(typeof(TestType));
            TestPublicNestedTypesWithInheritedArePropagated(typeof(TestType));
            TestPublicParameterlessConstructorIsNotPropagated(typeof(TestType));
            TestPublicPropertiesPropagated(typeof(TestType));

            TestNonPublicConstructorsAreNotPropagated(typeof(TestType));
            TestNonPublicConstructorsWithInheritedArePropagated(typeof(TestType));
            TestNonPublicEventsAreNotPropagated(typeof(TestType));
            TestNonPublicEventsWithInheritedArePropagated(typeof(TestType));
            TestNonPublicFieldsAreNotPropagated(typeof(TestType));
            TestNonPublicFieldsWithInheritedArePropagated(typeof(TestType));
            TestNonPublicMethodsAreNotPropagated(typeof(TestType));
            TestNonPublicMethodsWithInheritedArePropagated(typeof(TestType));
            TestNonPublicNestedTypesAreNotPropagated(typeof(TestType));
            TestNonPublicNestedTypesWithInheritedArePropagated(typeof(TestType));
            TestNonPublicPropertiesAreNotPropagated(typeof(TestType));
            TestNonPublicPropertiesWithInheritedArePropagated(typeof(TestType));

            TestInterfacesPropagated(typeof(TestType));

            TestCombinationOfPublicsIsPropagated(typeof(TestType));
            TestCombinationOfNonPublicsIsNotPropagated(typeof(TestType));
            TestCombinationOfPublicAndNonPublicsPropagatesPublicOnly(typeof(TestType));

            TestNoAnnotation(typeof(TestType));
            TestAnnotatedAndUnannotated(typeof(TestType), typeof(TestType), 0);
            TestNull();
            TestNoValue();

            Mixed_Derived.Test(typeof(TestType), 0);

            LoopPatterns.Test();
        }

        static void TestAllPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.All)] Type derivedType)
        {
            derivedType.BaseType.RequiresAll();
        }

        class AllPropagatedWithDerivedClass
        {
            [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions.RequiresAll) + "(Type)", nameof(TestSystemTypeBase.BaseType) + ".get", Tool.Analyzer, "https://github.com/dotnet/linker/issues/2673")]
            static void TestAllPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.All)] TestSystemTypeBase derivedType)
            {
                derivedType.BaseType.RequiresAll();
            }

            // This is a very special case - normally there's basically no way to "new up" a Type instance via the "new" operator.
            // The trimming tools and analyzer see an unknown value and thus warns that it doesn't fulfill the All annotation.
            [ExpectedWarning("IL2062", nameof(TestAllPropagated))]
            public static void Test()
            {
                TestAllPropagated(new TestSystemTypeBase());
            }
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicConstructors))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicConstructorsWithInherited))]
        static void TestPublicConstructorsAreNotPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicConstructors)] Type derivedType)
        {
            derivedType.BaseType.RequiresPublicConstructors();
            derivedType.BaseType.RequiresPublicConstructorsWithInherited();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicConstructors))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicConstructorsWithInherited))]
        static void TestPublicConstructorsWithInheritedArePropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypesEx.PublicConstructorsWithInherited)] Type derivedType)
        {
            derivedType.BaseType.RequiresPublicConstructors();
            derivedType.BaseType.RequiresPublicConstructorsWithInherited();

            // Should warn
            derivedType.BaseType.RequiresNonPublicConstructors();

            // Should warn
            derivedType.BaseType.RequiresNonPublicConstructorsWithInherited();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicMethods))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicEvents))]
        static void TestPublicEventsPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicEvents)] Type derivedType)
        {
            derivedType.BaseType.RequiresPublicEvents();

            // Should warn
            derivedType.BaseType.RequiresPublicMethods();

            // Should warn
            derivedType.BaseType.RequiresNonPublicEvents();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicMethods))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicFields))]
        static void TestPublicFieldsPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicFields)] Type derivedType)
        {
            derivedType.BaseType.RequiresPublicFields();

            // Should warn
            derivedType.BaseType.RequiresPublicMethods();

            // Should warn
            derivedType.BaseType.RequiresNonPublicFields();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicProperties))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicMethods))]
        static void TestPublicMethodsPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicMethods)] Type derivedType)
        {
            derivedType.BaseType.RequiresPublicMethods();

            // Should warn
            derivedType.BaseType.RequiresPublicProperties();

            // Should warn
            derivedType.BaseType.RequiresNonPublicMethods();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicNestedTypes))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicNestedTypesWithInherited))]
        static void TestPublicNestedTypesAreNotPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicNestedTypes)] Type derivedType)
        {
            derivedType.BaseType.RequiresPublicNestedTypes();
            derivedType.BaseType.RequiresPublicNestedTypesWithInherited();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicNestedTypes))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicNestedTypesWithInherited))]
        static void TestPublicNestedTypesWithInheritedArePropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypesEx.PublicNestedTypesWithInherited)] Type derivedType)
        {
            derivedType.BaseType.RequiresPublicNestedTypes();
            derivedType.BaseType.RequiresPublicNestedTypesWithInherited();

            // Should warn
            derivedType.BaseType.RequiresNonPublicNestedTypes();

            // Should warn
            derivedType.BaseType.RequiresNonPublicNestedTypesWithInherited();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicParameterlessConstructor))]
        static void TestPublicParameterlessConstructorIsNotPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicParameterlessConstructor)] Type derivedType)
        {
            derivedType.BaseType.RequiresPublicParameterlessConstructor();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicMethods))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicProperties))]
        static void TestPublicPropertiesPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicProperties)] Type derivedType)
        {
            derivedType.BaseType.RequiresPublicProperties();

            // Should warn
            derivedType.BaseType.RequiresPublicMethods();

            // Should warn
            derivedType.BaseType.RequiresNonPublicProperties();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicConstructors))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicConstructorsWithInherited))]
        static void TestNonPublicConstructorsAreNotPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.NonPublicConstructors)] Type derivedType)
        {
            derivedType.BaseType.RequiresNonPublicConstructors();
            derivedType.BaseType.RequiresNonPublicConstructorsWithInherited();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicConstructors))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicConstructorsWithInherited))]
        static void TestNonPublicConstructorsWithInheritedArePropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypesEx.NonPublicConstructorsWithInherited)] Type derivedType)
        {
            derivedType.BaseType.RequiresNonPublicConstructors();
            derivedType.BaseType.RequiresNonPublicConstructorsWithInherited();

            // Should warn
            derivedType.BaseType.RequiresPublicConstructors();

            // Should warn
            derivedType.BaseType.RequiresPublicConstructorsWithInherited();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicEvents))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicEventsWithInherited))]
        static void TestNonPublicEventsAreNotPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.NonPublicEvents)] Type derivedType)
        {
            derivedType.BaseType.RequiresNonPublicEvents();
            derivedType.BaseType.RequiresNonPublicEventsWithInherited();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicEvents))]
        static void TestNonPublicEventsWithInheritedArePropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypesEx.NonPublicEventsWithInherited)] Type derivedType)
        {
            derivedType.BaseType.RequiresNonPublicEvents();
            derivedType.BaseType.RequiresNonPublicEventsWithInherited();

            // Should warn
            derivedType.BaseType.RequiresPublicEvents();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicFields))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicFieldsWithInherited))]
        static void TestNonPublicFieldsAreNotPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.NonPublicFields)] Type derivedType)
        {
            derivedType.BaseType.RequiresNonPublicFields();
            derivedType.BaseType.RequiresNonPublicFieldsWithInherited();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicFields))]
        static void TestNonPublicFieldsWithInheritedArePropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypesEx.NonPublicFieldsWithInherited)] Type derivedType)
        {
            derivedType.BaseType.RequiresNonPublicFields();
            derivedType.BaseType.RequiresNonPublicFieldsWithInherited();

            // Should warn
            derivedType.BaseType.RequiresPublicFields();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicMethods))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicMethodsWithInherited))]
        static void TestNonPublicMethodsAreNotPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.NonPublicMethods)] Type derivedType)
        {
            derivedType.BaseType.RequiresNonPublicMethods();
            derivedType.BaseType.RequiresNonPublicMethodsWithInherited();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicMethods))]
        static void TestNonPublicMethodsWithInheritedArePropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypesEx.NonPublicMethodsWithInherited)] Type derivedType)
        {
            derivedType.BaseType.RequiresNonPublicMethods();
            derivedType.BaseType.RequiresNonPublicMethodsWithInherited();

            // Should warn
            derivedType.BaseType.RequiresPublicMethods();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicNestedTypes))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicNestedTypesWithInherited))]
        static void TestNonPublicNestedTypesAreNotPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.NonPublicNestedTypes)] Type derivedType)
        {
            derivedType.BaseType.RequiresNonPublicNestedTypes();
            derivedType.BaseType.RequiresNonPublicNestedTypesWithInherited();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicNestedTypes))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicNestedTypesWithInherited))]
        static void TestNonPublicNestedTypesWithInheritedArePropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypesEx.NonPublicNestedTypesWithInherited)] Type derivedType)
        {
            derivedType.BaseType.RequiresNonPublicNestedTypes();
            derivedType.BaseType.RequiresNonPublicNestedTypesWithInherited();

            // Should warn
            derivedType.BaseType.RequiresPublicNestedTypes();

            // Should warn
            derivedType.BaseType.RequiresPublicNestedTypesWithInherited();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicProperties))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicPropertiesWithInherited))]
        static void TestNonPublicPropertiesAreNotPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.NonPublicProperties)] Type derivedType)
        {
            derivedType.BaseType.RequiresNonPublicProperties();
            derivedType.BaseType.RequiresNonPublicPropertiesWithInherited();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicProperties))]
        static void TestNonPublicPropertiesWithInheritedArePropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypesEx.NonPublicPropertiesWithInherited)] Type derivedType)
        {
            derivedType.BaseType.RequiresNonPublicProperties();
            derivedType.BaseType.RequiresNonPublicPropertiesWithInherited();

            // Should warn
            derivedType.BaseType.RequiresPublicProperties();
        }

        static void TestInterfacesPropagated([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.Interfaces)] Type derivedType)
        {
            derivedType.BaseType.RequiresInterfaces();
        }

        static void TestCombinationOfPublicsIsPropagated(
            [DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicMethods | DynamicallyAccessedMemberTypes.PublicProperties)] Type derivedType)
        {
            derivedType.BaseType.RequiresPublicMethods();
            derivedType.BaseType.RequiresPublicProperties();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicMethods))]
        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicProperties))]
        static void TestCombinationOfNonPublicsIsNotPropagated(
            [DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.NonPublicMethods | DynamicallyAccessedMemberTypes.NonPublicProperties)] Type derivedType)
        {
            derivedType.BaseType.RequiresNonPublicMethods();
            derivedType.BaseType.RequiresNonPublicProperties();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresNonPublicMethods))]
        static void TestCombinationOfPublicAndNonPublicsPropagatesPublicOnly(
            [DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.NonPublicMethods | DynamicallyAccessedMemberTypes.PublicProperties)] Type derivedType)
        {
            derivedType.BaseType.RequiresNonPublicMethods();
            derivedType.BaseType.RequiresPublicProperties();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicMethods))]
        static void TestNoAnnotation(Type derivedType)
        {
            derivedType.BaseType.RequiresPublicMethods();
        }

        [ExpectedWarning("IL2072", nameof(DataFlowTypeExtensions) + "." + nameof(DataFlowTypeExtensions.RequiresPublicMethods))]
        static void TestAnnotatedAndUnannotated(
            Type derivedTypeOne,
            [DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicMethods)] Type derivedTypeTwo,
            int number)
        {
            Type type = number > 0 ? derivedTypeOne : derivedTypeTwo;
            type.BaseType.RequiresPublicMethods();
        }

        static void TestNull()
        {
            Type type = null;
            type.BaseType.RequiresPublicMethods();
        }

        static void TestNoValue()
        {
            Type t = null;
            Type noValue = Type.GetTypeFromHandle(t.TypeHandle);
            // No warning because the above throws an exception at runtime.
            noValue.BaseType.RequiresPublicMethods();
        }

        class Mixed_Base
        {
            public static void PublicMethod() { }
            private static void PrivateMethod() { }
        }

        class Mixed_Derived : Mixed_Base
        {
            public static void Test(
                [DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicMethods)] Type typeWithPublicMethods,
                int number)
            {
                Type type;
                switch (number)
                {
                    case 0:
                        type = typeof(TestType);
                        break;
                    case 1:
                        type = typeof(Mixed_Derived);
                        break;
                    case 2:
                        type = typeWithPublicMethods;
                        break;
                    default:
                        type = null;
                        break;
                }

                type.BaseType.RequiresPublicMethods();
            }
        }

        class LoopPatterns
        {
            static void EnumerateInterfacesOnBaseTypes([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.Interfaces)] Type type)
            {
                Type? t = type;
                while (t != null)
                {
                    Type[] interfaces = t.GetInterfaces();
                    t = t.BaseType;
                }
            }

            [ExpectedWarning("IL2070")]
            [ExpectedWarning("IL2075", Tool.Analyzer, "")] // ILLink doesn't implement backward branches data flow yet
            static void EnumerateInterfacesOnBaseTypes_Unannotated(Type type)
            {
                Type? t = type;
                while (t != null)
                {
                    Type[] interfaces = t.GetInterfaces();
                    t = t.BaseType;
                }
            }

            // Can only work with All annotation as NonPublicProperties doesn't propagate to base types
            static void EnumeratePrivatePropertiesOnBaseTypes([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.All)] Type type)
            {
                const BindingFlags DeclaredOnlyLookup = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static | BindingFlags.DeclaredOnly;
                Type? t = type;
                while (t != null)
                {
                    t.GetProperties(DeclaredOnlyLookup).GetEnumerator();
                    t = t.BaseType;
                }
            }

            // Can only work with All annotation as NonPublicProperties doesn't propagate to base types
            static void EnumeratePrivatePropertiesOnBaseTypesWithForeach([DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.All)] Type type)
            {
                const BindingFlags DeclaredOnlyLookup = BindingFlags.Public | BindingFlags.NonPublic | BindingFlags.Instance | BindingFlags.Static | BindingFlags.DeclaredOnly;
                Type? t = type;
                while (t != null)
                {
                    foreach (var p in t.GetProperties(DeclaredOnlyLookup))
                    {
                        // Do nothing
                    }
                    t = t.BaseType;
                }
            }

            public static void Test()
            {
                EnumerateInterfacesOnBaseTypes(typeof(TestType));
                EnumerateInterfacesOnBaseTypes_Unannotated(typeof(TestType));

                EnumeratePrivatePropertiesOnBaseTypes(typeof(TestType));
                EnumeratePrivatePropertiesOnBaseTypesWithForeach(typeof(TestType));
            }
        }

        class TestType
        {
        }
    }
}
