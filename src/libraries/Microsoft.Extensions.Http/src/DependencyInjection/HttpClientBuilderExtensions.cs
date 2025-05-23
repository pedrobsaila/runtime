// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System;
using System.Collections.Generic;
using System.Diagnostics;
using System.Diagnostics.CodeAnalysis;
using System.Linq;
using System.Net.Http;
using System.Runtime.Versioning;
using System.Threading;
using Microsoft.Extensions.Http;
using Microsoft.Extensions.Options;

namespace Microsoft.Extensions.DependencyInjection
{
    /// <summary>
    /// Extension methods for configuring an <see cref="IHttpClientBuilder"/>.
    /// </summary>
    public static partial class HttpClientBuilderExtensions
    {
        /// <summary>
        /// Adds a delegate that will be used to configure a named <see cref="HttpClient"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="configureClient">A delegate that is used to configure an <see cref="HttpClient"/>.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        public static IHttpClientBuilder ConfigureHttpClient(this IHttpClientBuilder builder, Action<HttpClient> configureClient)
        {
            ArgumentNullException.ThrowIfNull(builder);
            ArgumentNullException.ThrowIfNull(configureClient);

            builder.Services.Configure<HttpClientFactoryOptions>(builder.Name, options => options.HttpClientActions.Add(configureClient));

            return builder;
        }

        /// <summary>
        /// Adds a delegate that will be used to configure a named <see cref="HttpClient"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="configureClient">A delegate that is used to configure an <see cref="HttpClient"/>.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        /// <remarks>
        /// The <see cref="IServiceProvider"/> provided to <paramref name="configureClient"/> will be the
        /// same application's root service provider instance.
        /// </remarks>
        public static IHttpClientBuilder ConfigureHttpClient(this IHttpClientBuilder builder, Action<IServiceProvider, HttpClient> configureClient)
        {
            ArgumentNullException.ThrowIfNull(builder);
            ArgumentNullException.ThrowIfNull(configureClient);

            builder.Services.AddTransient<IConfigureOptions<HttpClientFactoryOptions>>(services =>
            {
                return new ConfigureNamedOptions<HttpClientFactoryOptions>(builder.Name, (options) =>
                {
                    options.HttpClientActions.Add(client => configureClient(services, client));
                });
            });

            return builder;
        }

        /// <summary>
        /// Adds a delegate that will be used to create an additional message handler for a named <see cref="HttpClient"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="configureHandler">A delegate that is used to create a <see cref="DelegatingHandler"/>.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        /// <remarks>
        /// The <paramref name="configureHandler"/> delegate should return a new instance of the message handler each time it
        /// is invoked.
        /// </remarks>
        public static IHttpClientBuilder AddHttpMessageHandler(this IHttpClientBuilder builder, Func<DelegatingHandler> configureHandler)
        {
            ArgumentNullException.ThrowIfNull(builder);
            ArgumentNullException.ThrowIfNull(configureHandler);

            builder.Services.Configure<HttpClientFactoryOptions>(builder.Name, options =>
            {
                options.HttpMessageHandlerBuilderActions.Add(b => b.AdditionalHandlers.Add(configureHandler()));
            });

            return builder;
        }

        /// <summary>
        /// Adds a delegate that will be used to create an additional message handler for a named <see cref="HttpClient"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="configureHandler">A delegate that is used to create a <see cref="DelegatingHandler"/>.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        /// <remarks>
        /// <para>
        /// The <paramref name="configureHandler"/> delegate should return a new instance of the message handler each time it
        /// is invoked.
        /// </para>
        /// <para>
        /// The <see cref="IServiceProvider"/> argument provided to <paramref name="configureHandler"/> will be
        /// a reference to a scoped service provider that shares the lifetime of the handler being constructed.
        /// </para>
        /// </remarks>
        public static IHttpClientBuilder AddHttpMessageHandler(this IHttpClientBuilder builder, Func<IServiceProvider, DelegatingHandler> configureHandler)
        {
            ArgumentNullException.ThrowIfNull(builder);
            ArgumentNullException.ThrowIfNull(configureHandler);

            builder.Services.Configure<HttpClientFactoryOptions>(builder.Name, options =>
            {
                options.HttpMessageHandlerBuilderActions.Add(b => b.AdditionalHandlers.Add(configureHandler(b.Services)));
            });

            return builder;
        }

        /// <summary>
        /// Adds an additional message handler from the dependency injection container for a named <see cref="HttpClient"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        /// <typeparam name="THandler">
        /// The type of the <see cref="DelegatingHandler"/>. The handler type must be registered as a transient service.
        /// </typeparam>
        /// <remarks>
        /// <para>
        /// The <typeparamref name="THandler"/> will be resolved from a scoped service provider that shares
        /// the lifetime of the handler being constructed.
        /// </para>
        /// </remarks>
        public static IHttpClientBuilder AddHttpMessageHandler<THandler>(this IHttpClientBuilder builder)
            where THandler : DelegatingHandler
        {
            ArgumentNullException.ThrowIfNull(builder);

            builder.Services.Configure<HttpClientFactoryOptions>(builder.Name, options =>
            {
                options.HttpMessageHandlerBuilderActions.Add(b => b.AdditionalHandlers.Add(b.Services.GetRequiredService<THandler>()));
            });

            return builder;
        }

        /// <summary>
        /// Adds a delegate that will be used to configure the primary <see cref="HttpMessageHandler"/> for a
        /// named <see cref="HttpClient"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="configureHandler">A delegate that is used to create an <see cref="HttpMessageHandler"/>.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        /// <remarks>
        /// The <paramref name="configureHandler"/> delegate should return a new instance of the message handler each time it
        /// is invoked.
        /// </remarks>
        public static IHttpClientBuilder ConfigurePrimaryHttpMessageHandler(this IHttpClientBuilder builder, Func<HttpMessageHandler> configureHandler)
        {
            ArgumentNullException.ThrowIfNull(builder);
            ArgumentNullException.ThrowIfNull(configureHandler);

            builder.Services.Configure<HttpClientFactoryOptions>(builder.Name, options =>
            {
                options.HttpMessageHandlerBuilderActions.Add(b => b.PrimaryHandler = configureHandler());
            });

            return builder;
        }

        /// <summary>
        /// Adds a delegate that will be used to configure the primary <see cref="HttpMessageHandler"/> for a
        /// named <see cref="HttpClient"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="configureHandler">A delegate that is used to create an <see cref="HttpMessageHandler"/>.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        /// <remarks>
        /// <para>
        /// The <paramref name="configureHandler"/> delegate should return a new instance of the message handler each time it
        /// is invoked.
        /// </para>
        /// <para>
        /// The <see cref="IServiceProvider"/> argument provided to <paramref name="configureHandler"/> will be
        /// a reference to a scoped service provider that shares the lifetime of the handler being constructed.
        /// </para>
        /// </remarks>
        public static IHttpClientBuilder ConfigurePrimaryHttpMessageHandler(this IHttpClientBuilder builder, Func<IServiceProvider, HttpMessageHandler> configureHandler)
        {
            ArgumentNullException.ThrowIfNull(builder);
            ArgumentNullException.ThrowIfNull(configureHandler);

            builder.Services.Configure<HttpClientFactoryOptions>(builder.Name, options =>
            {
                options.HttpMessageHandlerBuilderActions.Add(b => b.PrimaryHandler = configureHandler(b.Services));
            });

            return builder;
        }

        /// <summary>
        /// Configures the primary <see cref="HttpMessageHandler"/> from the dependency injection container
        /// for a named <see cref="HttpClient"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        /// <typeparam name="THandler">
        /// The type of the <see cref="DelegatingHandler"/>. The handler type must be registered as a transient service.
        /// </typeparam>
        /// <remarks>
        /// <para>
        /// The <typeparamref name="THandler"/> will be resolved from a scoped service provider that shares
        /// the lifetime of the handler being constructed.
        /// </para>
        /// </remarks>
        public static IHttpClientBuilder ConfigurePrimaryHttpMessageHandler<THandler>(this IHttpClientBuilder builder)
            where THandler : HttpMessageHandler
        {
            ArgumentNullException.ThrowIfNull(builder);

            builder.Services.Configure<HttpClientFactoryOptions>(builder.Name, options =>
            {
                options.HttpMessageHandlerBuilderActions.Add(b => b.PrimaryHandler = b.Services.GetRequiredService<THandler>());
            });

            return builder;
        }

        /// <summary>
        /// Adds a delegate that will be used to configure the primary <see cref="HttpMessageHandler"/> for a
        /// named <see cref="HttpClient"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="configureHandler">A delegate that is used to configure a previously set or default primary <see cref="HttpMessageHandler"/>.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        /// <remarks>
        /// <para>
        /// The <see cref="IServiceProvider"/> argument provided to <paramref name="configureHandler"/> will be
        /// a reference to a scoped service provider that shares the lifetime of the handler being constructed.
        /// </para>
        /// </remarks>
        public static IHttpClientBuilder ConfigurePrimaryHttpMessageHandler(this IHttpClientBuilder builder, Action<HttpMessageHandler, IServiceProvider> configureHandler)
        {
            ArgumentNullException.ThrowIfNull(builder);
            ArgumentNullException.ThrowIfNull(configureHandler);

            builder.Services.Configure<HttpClientFactoryOptions>(builder.Name, options =>
            {
                options.HttpMessageHandlerBuilderActions.Add(b => configureHandler(b.PrimaryHandler, b.Services));
            });

            return builder;
        }

        /// <summary>
        /// Adds a delegate that will be used to configure message handlers using <see cref="HttpMessageHandlerBuilder"/>
        /// for a named <see cref="HttpClient"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="configureBuilder">A delegate that is used to configure an <see cref="HttpMessageHandlerBuilder"/>.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        [Obsolete("This method has been deprecated. Use ConfigurePrimaryHttpMessageHandler or ConfigureAdditionalHttpMessageHandlers instead.")]
        public static IHttpClientBuilder ConfigureHttpMessageHandlerBuilder(this IHttpClientBuilder builder, Action<HttpMessageHandlerBuilder> configureBuilder)
        {
            ArgumentNullException.ThrowIfNull(builder);
            ArgumentNullException.ThrowIfNull(configureBuilder);

            builder.Services.Configure<HttpClientFactoryOptions>(builder.Name, options => options.HttpMessageHandlerBuilderActions.Add(configureBuilder));

            return builder;
        }

#if NET
        /// <summary>
        /// Adds or updates <see cref="SocketsHttpHandler"/> as a primary handler for a named <see cref="HttpClient"/>. If provided,
        /// also adds a delegate that will be used to configure the primary <see cref="SocketsHttpHandler"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="configureHandler">Optional delegate that is used to configure the primary <see cref="SocketsHttpHandler"/>.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        /// <remarks>
        /// <para>
        /// If a primary handler was already set to be <see cref="SocketsHttpHandler"/> by previously calling, for example,
        /// <see cref="ConfigurePrimaryHttpMessageHandler(IHttpClientBuilder, Func{HttpMessageHandler})"/> or
        /// <see cref="UseSocketsHttpHandler(IHttpClientBuilder, Action{ISocketsHttpHandlerBuilder})"/>, then the passed <paramref name="configureHandler"/>
        /// delegate will be applied to the existing instance. Otherwise, a new instance of <see cref="SocketsHttpHandler"/> will be created.
        /// </para>
        /// </remarks>
        [UnsupportedOSPlatform("browser")]
        public static IHttpClientBuilder UseSocketsHttpHandler(this IHttpClientBuilder builder, Action<SocketsHttpHandler, IServiceProvider>? configureHandler = null)
        {
            ArgumentNullException.ThrowIfNull(builder);

            builder.Services.Configure<HttpClientFactoryOptions>(builder.Name, options =>
            {
                options.HttpMessageHandlerBuilderActions.Add(b =>
                {
                    if (b.PrimaryHandler is not SocketsHttpHandler handler)
                    {
                        handler = new SocketsHttpHandler();
                    }
                    configureHandler?.Invoke(handler, b.Services);
                    b.PrimaryHandler = handler;
                });
            });

            return builder;
        }

        /// <summary>
        /// Adds or updates <see cref="SocketsHttpHandler"/> as a primary handler for a named <see cref="HttpClient"/>
        /// and configures it using <see cref="ISocketsHttpHandlerBuilder"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="configureBuilder">Delegate that is used to set up the configuration of the primary <see cref="SocketsHttpHandler"/>
        /// on <see cref="ISocketsHttpHandlerBuilder"/> that will later be applied on the primary handler during its creation.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        /// <remarks>
        /// <para>
        /// If a primary handler was already set to be <see cref="SocketsHttpHandler"/> by previously calling, for example,
        /// <see cref="ConfigurePrimaryHttpMessageHandler(IHttpClientBuilder, Func{HttpMessageHandler})"/> or
        /// <see cref="UseSocketsHttpHandler(IHttpClientBuilder, Action{ISocketsHttpHandlerBuilder})"/>, then the configuration set on
        /// <see cref="ISocketsHttpHandlerBuilder"/> will be applied to the existing instance. Otherwise, a new instance of
        /// <see cref="SocketsHttpHandler"/> will be created.
        /// </para>
        /// </remarks>
        [UnsupportedOSPlatform("browser")]
        public static IHttpClientBuilder UseSocketsHttpHandler(this IHttpClientBuilder builder, Action<ISocketsHttpHandlerBuilder> configureBuilder)
        {
            ArgumentNullException.ThrowIfNull(builder);

            UseSocketsHttpHandler(builder);
            configureBuilder(new DefaultSocketsHttpHandlerBuilder(builder.Services, builder.Name));

            return builder;
        }
#endif

        /// <summary>
        /// Configures a binding between the <typeparamref name="TClient" /> type and the named <see cref="HttpClient"/>
        /// associated with the <see cref="IHttpClientBuilder"/>.
        /// </summary>
        /// <typeparam name="TClient">
        /// The type of the typed client. The type specified will be registered in the service collection as
        /// a transient service. See <see cref="ITypedHttpClientFactory{TClient}" /> for more details about authoring typed clients.
        /// </typeparam>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <remarks>
        /// <para>
        /// <typeparamref name="TClient"/> instances constructed with the appropriate <see cref="HttpClient" />
        /// can be retrieved from <see cref="IServiceProvider.GetService(Type)" /> (and related methods) by providing
        /// <typeparamref name="TClient"/> as the service type.
        /// </para>
        /// <para>
        /// Calling <see cref="HttpClientBuilderExtensions.AddTypedClient{TClient}(IHttpClientBuilder)"/> will register a typed
        /// client binding that creates <typeparamref name="TClient"/> using the <see cref="ITypedHttpClientFactory{TClient}" />.
        /// </para>
        /// <para>
        /// The typed client's service dependencies will be resolved from the same service provider
        /// that is used to resolve the typed client. It is not possible to access services from the
        /// scope bound to the message handler, which is managed independently.
        /// </para>
        /// </remarks>
        public static IHttpClientBuilder AddTypedClient<[DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicConstructors)] TClient>(
            this IHttpClientBuilder builder)
            where TClient : class
        {
            ArgumentNullException.ThrowIfNull(builder);

            return AddTypedClientCore<TClient>(builder, validateSingleType: false);
        }

        internal static IHttpClientBuilder AddTypedClientCore<[DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicConstructors)] TClient>(
            this IHttpClientBuilder builder, bool validateSingleType)
            where TClient : class
        {
            ReserveClient(builder, typeof(TClient), builder.Name, validateSingleType);

            builder.Services.AddTransient(s => AddTransientHelper<TClient>(s, builder));

            return builder;
        }

        [UnconditionalSuppressMessage("ReflectionAnalysis", "IL2091:UnrecognizedReflectionPattern",
            Justification = "Workaround for https://github.com/mono/linker/issues/1416. Outer method has been annotated with DynamicallyAccessedMembers.")]
        private static TClient AddTransientHelper<TClient>(IServiceProvider s, IHttpClientBuilder builder) where TClient : class
        {
            IHttpClientFactory httpClientFactory = s.GetRequiredService<IHttpClientFactory>();
            HttpClient httpClient = httpClientFactory.CreateClient(builder.Name);

            ITypedHttpClientFactory<TClient> typedClientFactory = s.GetRequiredService<ITypedHttpClientFactory<TClient>>();
            return typedClientFactory.CreateClient(httpClient);
        }

        /// <summary>
        /// Configures a binding between the <typeparamref name="TClient" /> type and the named <see cref="HttpClient"/>
        /// associated with the <see cref="IHttpClientBuilder"/>. The created instances will be of type
        /// <typeparamref name="TImplementation"/>.
        /// </summary>
        /// <typeparam name="TClient">
        /// The declared type of the typed client. They type specified will be registered in the service collection as
        /// a transient service. See <see cref="ITypedHttpClientFactory{TImplementation}" /> for more details about authoring typed clients.
        /// </typeparam>
        /// <typeparam name="TImplementation">
        /// The implementation type of the typed client. The type specified by will be instantiated by the
        /// <see cref="ITypedHttpClientFactory{TImplementation}"/>.
        /// </typeparam>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <remarks>
        /// <para>
        /// <typeparamref name="TClient"/> instances constructed with the appropriate <see cref="HttpClient" />
        /// can be retrieved from <see cref="IServiceProvider.GetService(Type)" /> (and related methods) by providing
        /// <typeparamref name="TClient"/> as the service type.
        /// </para>
        /// <para>
        /// Calling <see cref="HttpClientBuilderExtensions.AddTypedClient{TClient,TImplementation}(IHttpClientBuilder)"/>
        /// will register a typed client binding that creates <typeparamref name="TImplementation"/> using the
        /// <see cref="ITypedHttpClientFactory{TImplementation}" />.
        /// </para>
        /// <para>
        /// The typed client's service dependencies will be resolved from the same service provider
        /// that is used to resolve the typed client. It is not possible to access services from the
        /// scope bound to the message handler, which is managed independently.
        /// </para>
        /// </remarks>
        public static IHttpClientBuilder AddTypedClient<TClient, [DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicConstructors)] TImplementation>(
            this IHttpClientBuilder builder)
            where TClient : class
            where TImplementation : class, TClient
        {
            ArgumentNullException.ThrowIfNull(builder);

            return AddTypedClientCore<TClient, TImplementation>(builder, validateSingleType: false);
        }

        internal static IHttpClientBuilder AddTypedClientCore<TClient, [DynamicallyAccessedMembers(DynamicallyAccessedMemberTypes.PublicConstructors)] TImplementation>(
            this IHttpClientBuilder builder, bool validateSingleType)
            where TClient : class
            where TImplementation : class, TClient
        {
            ReserveClient(builder, typeof(TClient), builder.Name, validateSingleType);

            builder.Services.AddTransient(s => AddTransientHelper<TClient, TImplementation>(s, builder));

            return builder;
        }

        [UnconditionalSuppressMessage("ReflectionAnalysis", "IL2091:UnrecognizedReflectionPattern",
            Justification = "Workaround for https://github.com/mono/linker/issues/1416. Outer method has been annotated with DynamicallyAccessedMembers.")]
        private static TClient AddTransientHelper<TClient, TImplementation>(IServiceProvider s, IHttpClientBuilder builder) where TClient : class where TImplementation : class, TClient
        {
            IHttpClientFactory httpClientFactory = s.GetRequiredService<IHttpClientFactory>();
            HttpClient httpClient = httpClientFactory.CreateClient(builder.Name);

            ITypedHttpClientFactory<TImplementation> typedClientFactory = s.GetRequiredService<ITypedHttpClientFactory<TImplementation>>();
            return typedClientFactory.CreateClient(httpClient);
        }

        /// <summary>
        /// Configures a binding between the <typeparamref name="TClient" /> type and the named <see cref="HttpClient"/>
        /// associated with the <see cref="IHttpClientBuilder"/>.
        /// </summary>
        /// <typeparam name="TClient">
        /// The type of the typed client. They type specified will be registered in the service collection as
        /// a transient service.
        /// </typeparam>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="factory">A factory function that will be used to construct the typed client.</param>
        /// <remarks>
        /// <para>
        /// <typeparamref name="TClient"/> instances constructed with the appropriate <see cref="HttpClient" />
        /// can be retrieved from <see cref="IServiceProvider.GetService(Type)" /> (and related methods) by providing
        /// <typeparamref name="TClient"/> as the service type.
        /// </para>
        /// <para>
        /// Calling <see cref="HttpClientBuilderExtensions.AddTypedClient{TClient}(IHttpClientBuilder,Func{HttpClient,TClient})"/>
        /// will register a typed client binding that creates <typeparamref name="TClient"/> using the provided factory function.
        /// </para>
        /// </remarks>
        public static IHttpClientBuilder AddTypedClient<TClient>(this IHttpClientBuilder builder, Func<HttpClient, TClient> factory)
            where TClient : class
        {
            ArgumentNullException.ThrowIfNull(builder);
            ArgumentNullException.ThrowIfNull(factory);

            return AddTypedClientCore<TClient>(builder, factory, validateSingleType: false);
        }

        internal static IHttpClientBuilder AddTypedClientCore<TClient>(this IHttpClientBuilder builder, Func<HttpClient, TClient> factory, bool validateSingleType)
            where TClient : class
        {
            ReserveClient(builder, typeof(TClient), builder.Name, validateSingleType);

            builder.Services.AddTransient<TClient>(s =>
            {
                IHttpClientFactory httpClientFactory = s.GetRequiredService<IHttpClientFactory>();
                HttpClient httpClient = httpClientFactory.CreateClient(builder.Name);

                return factory(httpClient);
            });

            return builder;
        }

        /// <summary>
        /// Configures a binding between the <typeparamref name="TClient" /> type and the named <see cref="HttpClient"/>
        /// associated with the <see cref="IHttpClientBuilder"/>.
        /// </summary>
        /// <typeparam name="TClient">
        /// The type of the typed client. They type specified will be registered in the service collection as
        /// a transient service.
        /// </typeparam>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="factory">A factory function that will be used to construct the typed client.</param>
        /// <remarks>
        /// <para>
        /// <typeparamref name="TClient"/> instances constructed with the appropriate <see cref="HttpClient" />
        /// can be retrieved from <see cref="IServiceProvider.GetService(Type)" /> (and related methods) by providing
        /// <typeparamref name="TClient"/> as the service type.
        /// </para>
        /// <para>
        /// Calling <see cref="HttpClientBuilderExtensions.AddTypedClient{TClient}(IHttpClientBuilder,Func{HttpClient,IServiceProvider,TClient})"/>
        /// will register a typed client binding that creates <typeparamref name="TClient"/> using the provided factory function.
        /// </para>
        /// </remarks>
        public static IHttpClientBuilder AddTypedClient<TClient>(this IHttpClientBuilder builder, Func<HttpClient, IServiceProvider, TClient> factory)
            where TClient : class
        {
            ArgumentNullException.ThrowIfNull(builder);
            ArgumentNullException.ThrowIfNull(factory);

            return AddTypedClientCore<TClient>(builder, factory, validateSingleType: false);
        }

        internal static IHttpClientBuilder AddTypedClientCore<TClient>(this IHttpClientBuilder builder, Func<HttpClient, IServiceProvider, TClient> factory, bool validateSingleType)
            where TClient : class
        {
            ArgumentNullException.ThrowIfNull(builder);
            ArgumentNullException.ThrowIfNull(factory);

            ReserveClient(builder, typeof(TClient), builder.Name, validateSingleType);

            builder.Services.AddTransient<TClient>(s =>
            {
                IHttpClientFactory httpClientFactory = s.GetRequiredService<IHttpClientFactory>();
                HttpClient httpClient = httpClientFactory.CreateClient(builder.Name);

                return factory(httpClient, s);
            });

            return builder;
        }

        /// <summary>
        /// Sets the <see cref="Func{T, R}"/> which determines whether to redact the HTTP header value given its corresponding header name before logging.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="shouldRedactHeaderValue">The <see cref="Func{T, R}"/> which determines whether to redact the HTTP header value given its corresponding header name before logging.</param>
        /// <returns>The <see cref="IHttpClientBuilder"/>.</returns>
        /// <remarks>The provided <paramref name="shouldRedactHeaderValue"/> predicate will be evaluated for each header name when logging. If the predicate returns <see langword="true"/> then the header value will be replaced with a marker value <c>*</c> in logs; otherwise the header value will be logged.
        /// </remarks>
        public static IHttpClientBuilder RedactLoggedHeaders(this IHttpClientBuilder builder, Func<string, bool> shouldRedactHeaderValue)
        {
            ArgumentNullException.ThrowIfNull(builder);
            ArgumentNullException.ThrowIfNull(shouldRedactHeaderValue);

            builder.Services.Configure<HttpClientFactoryOptions>(builder.Name, options =>
            {
                options.ShouldRedactHeaderValue = shouldRedactHeaderValue;
            });

            return builder;
        }

        /// <summary>
        /// Sets the collection of HTTP headers names for which values should be redacted before logging.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="redactedLoggedHeaderNames">The collection of HTTP headers names for which values should be redacted before logging.</param>
        /// <returns>The <see cref="IHttpClientBuilder"/>.</returns>
        public static IHttpClientBuilder RedactLoggedHeaders(this IHttpClientBuilder builder, IEnumerable<string> redactedLoggedHeaderNames)
        {
            ArgumentNullException.ThrowIfNull(builder);
            ArgumentNullException.ThrowIfNull(redactedLoggedHeaderNames);

            builder.Services.Configure<HttpClientFactoryOptions>(builder.Name, options =>
            {
                var sensitiveHeaders = new HashSet<string>(redactedLoggedHeaderNames, StringComparer.OrdinalIgnoreCase);

                options.ShouldRedactHeaderValue = (header) => sensitiveHeaders.Contains(header);
            });

            return builder;
        }

        /// <summary>
        /// Sets the length of time that a <see cref="HttpMessageHandler"/> instance can be reused. Each named
        /// client can have its own configured handler lifetime value. The default value is two minutes. Set the lifetime to
        /// <see cref="Timeout.InfiniteTimeSpan"/> to disable handler expiry.
        /// </summary>
        /// <remarks>
        /// <para>
        /// The default implementation of <see cref="IHttpClientFactory"/> will pool the <see cref="HttpMessageHandler"/>
        /// instances created by the factory to reduce resource consumption. This setting configures the amount of time
        /// a handler can be pooled before it is scheduled for removal from the pool and disposal.
        /// </para>
        /// <para>
        /// Pooling of handlers is desirable as each handler typically manages its own underlying HTTP connections; creating
        /// more handlers than necessary can result in connection delays. Some handlers also keep connections open indefinitely
        /// which can prevent the handler from reacting to DNS changes. The value of <paramref name="handlerLifetime"/> should be
        /// chosen with an understanding of the application's requirement to respond to changes in the network environment.
        /// </para>
        /// <para>
        /// Expiry of a handler will not immediately dispose the handler. An expired handler is placed in a separate pool
        /// which is processed at intervals to dispose handlers only when they become unreachable. Using long-lived
        /// <see cref="HttpClient"/> instances will prevent the underlying <see cref="HttpMessageHandler"/> from being
        /// disposed until all references are garbage-collected.
        /// </para>
        /// </remarks>
        public static IHttpClientBuilder SetHandlerLifetime(this IHttpClientBuilder builder, TimeSpan handlerLifetime)
        {
            ArgumentNullException.ThrowIfNull(builder);

            if (handlerLifetime != Timeout.InfiniteTimeSpan && handlerLifetime < HttpClientFactoryOptions.MinimumHandlerLifetime)
            {
                throw new ArgumentException(SR.HandlerLifetime_InvalidValue, nameof(handlerLifetime));
            }

            builder.Services.Configure<HttpClientFactoryOptions>(builder.Name, options => options.HandlerLifetime = handlerLifetime);
            return builder;
        }

        /// <summary>
        /// Adds a delegate that will be used to configure additional message handlers using <see cref="HttpMessageHandlerBuilder"/>
        /// for a named <see cref="HttpClient"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="configureAdditionalHandlers">A delegate that is used to configure a collection of <see cref="DelegatingHandler"/>s.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        public static IHttpClientBuilder ConfigureAdditionalHttpMessageHandlers(this IHttpClientBuilder builder, Action<IList<DelegatingHandler>, IServiceProvider> configureAdditionalHandlers)
        {
            ArgumentNullException.ThrowIfNull(builder);
            ArgumentNullException.ThrowIfNull(configureAdditionalHandlers);

            builder.Services.Configure<HttpClientFactoryOptions>(builder.Name, options =>
            {
                options.HttpMessageHandlerBuilderActions.Add(b => configureAdditionalHandlers(b.AdditionalHandlers, b.Services));
            });

            return builder;
        }

        /// <summary>
        /// Registers a named <see cref="HttpClient"/> and the related handler pipeline <see cref="HttpMessageHandler"/> as keyed
        /// services with the client's name as the key, and a lifetime provided in the <paramref name="lifetime" /> parameter.
        /// By default, the lifetime is <see cref="ServiceLifetime.Scoped"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <param name="lifetime">Lifetime of the keyed services registered.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        /// <remarks>
        /// <para>
        /// A named client resolved from DI as a keyed service will behave similarly to a client you would create with <see cref="IHttpClientFactory.CreateClient(string)"/>. This
        /// means that the client will continue reusing the same <see cref="HttpMessageHandler"/> instance for the duration of <see cref="HttpClientFactoryOptions.HandlerLifetime"/>,
        /// and it will continue to use the separate, handler's DI scope instead of the scope it was resolved from.
        /// </para>
        /// <para>
        /// WARNING: Registering the client as a keyed <see cref="ServiceLifetime.Transient"/> service will lead to the <see cref="HttpClient"/> and <see cref="HttpMessageHandler"/>
        /// instances being captured by DI as both implement <see cref="IDisposable"/>. This might lead to memory leaks if the client is resolved multiple times within a
        /// <see cref="ServiceLifetime.Singleton"/> service.
        /// </para>
        /// <para>
        /// WARNING: In case of (1) a keyed <see cref="ServiceLifetime.Singleton"/> <see cref="HttpClient"/> registration, or (2) a keyed <see cref="ServiceLifetime.Transient"/>
        /// <see cref="HttpClient"/> injected into a <see cref="ServiceLifetime.Singleton"/> service, or (3) long-running application scopes,
        /// the <see cref="HttpClient"/> instances will get captured by a singleton or a long-running scope, so they will NOT be able to participate in the handler rotation,
        /// which can result in the loss of DNS changes. (This is a similar issue to the one with Typed Clients, that are registered as <see cref="ServiceLifetime.Transient"/> services.)
        /// </para>
        /// <para>
        /// If called twice with for a builder with the same name, the lifetime of the keyed service will be updated to the latest used <see cref="ServiceLifetime"/> value.
        /// </para>
        /// <para>
        /// If called for a typed client, only the related named client and handler will be registered as keyed. The typed client itself will continue to be registered as
        /// a transient service.
        /// </para>
        /// <para>
        /// If used in conjunction with <see cref="HttpClientFactoryServiceCollectionExtensions.ConfigureHttpClientDefaults(IServiceCollection, Action{IHttpClientBuilder})"/>,
        /// the key <see cref="KeyedService.AnyKey"/> is used, so any named <see cref="HttpClient"/> instance will be resolvable as a keyed service (unless explicitly opted-out
        /// from the keyed registration via <see cref="RemoveAsKeyed"/>).
        /// </para>
        /// </remarks>
        public static IHttpClientBuilder AddAsKeyed(this IHttpClientBuilder builder, ServiceLifetime lifetime = ServiceLifetime.Scoped)
        {
            ArgumentNullException.ThrowIfNull(builder);

            string? name = builder.Name;
            IServiceCollection services = builder.Services;
            HttpClientMappingRegistry registry = services.GetMappingRegistry();

            if (name == null)
            {
                registry.DefaultKeyedLifetime?.RemoveRegistration(services);

                registry.DefaultKeyedLifetime = new HttpClientKeyedLifetime(lifetime);
                registry.DefaultKeyedLifetime.AddRegistration(services);
            }
            else
            {
                if (registry.KeyedLifetimeMap.TryGetValue(name, out HttpClientKeyedLifetime? clientLifetime))
                {
                    clientLifetime.RemoveRegistration(services);
                }

                clientLifetime = new HttpClientKeyedLifetime(name, lifetime);
                registry.KeyedLifetimeMap[name] = clientLifetime;
                clientLifetime.AddRegistration(services);
            }

            return builder;
        }

        /// <summary>
        /// Removes the keyed registrations for the named <see cref="HttpClient"/> and <see cref="HttpMessageHandler"/>.
        /// </summary>
        /// <param name="builder">The <see cref="IHttpClientBuilder"/>.</param>
        /// <returns>An <see cref="IHttpClientBuilder"/> that can be used to configure the client.</returns>
        /// <remarks>
        /// <para>
        /// If used in conjunction with <see cref="HttpClientFactoryServiceCollectionExtensions.ConfigureHttpClientDefaults(IServiceCollection, Action{IHttpClientBuilder})"/>,
        /// it will only affect the previous "global" <see cref="KeyedService.AnyKey"/> registration, and won't affect the clients registered for a specific name
        /// with <see cref="AddAsKeyed"/>.
        /// </para>
        /// </remarks>
        public static IHttpClientBuilder RemoveAsKeyed(this IHttpClientBuilder builder)
        {
            ArgumentNullException.ThrowIfNull(builder);

            string? name = builder.Name;
            IServiceCollection services = builder.Services;
            HttpClientMappingRegistry registry = services.GetMappingRegistry();

            if (name == null)
            {
                registry.DefaultKeyedLifetime?.RemoveRegistration(services);
                registry.DefaultKeyedLifetime = HttpClientKeyedLifetime.Disabled;
            }
            else
            {
                if (registry.KeyedLifetimeMap.TryGetValue(name, out HttpClientKeyedLifetime? clientLifetime))
                {
                    clientLifetime.RemoveRegistration(services);
                }
                registry.KeyedLifetimeMap[name] = HttpClientKeyedLifetime.Disabled;
            }

            return builder;
        }

        // See comments on HttpClientMappingRegistry.
        private static void ReserveClient(IHttpClientBuilder builder, Type type, string name, bool validateSingleType)
        {
            if (builder.Name is null)
            {
                throw new InvalidOperationException($"{nameof(AddTypedClient)} isn't supported with {nameof(HttpClientFactoryServiceCollectionExtensions.ConfigureHttpClientDefaults)}.");
            }

            HttpClientMappingRegistry registry = GetMappingRegistry(builder.Services);

            // Check for same name registered to two types. This won't work because we rely on named options for the configuration.
            if (registry.NamedClientRegistrations.TryGetValue(name, out Type? otherType) &&

                // Allow using the same name with multiple types in some cases (see callers).
                validateSingleType &&

                // Allow registering the same name twice to the same type.
                type != otherType)
            {
                string message =
                    $"The HttpClient factory already has a registered client with the name '{name}', bound to the type '{otherType.FullName}'. " +
                    $"Client names are computed based on the type name without considering the namespace ('{otherType.Name}'). " +
                    $"Use an overload of AddHttpClient that accepts a string and provide a unique name to resolve the conflict.";
                throw new InvalidOperationException(message);
            }

            if (validateSingleType)
            {
                registry.NamedClientRegistrations[name] = type;
            }
        }

        internal static HttpClientMappingRegistry GetMappingRegistry(this IServiceCollection services)
        {
            var registry = (HttpClientMappingRegistry?)services.Single(sd => sd.ServiceType == typeof(HttpClientMappingRegistry)).ImplementationInstance;
            Debug.Assert(registry != null);
            return registry;
        }
    }
}
