// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System.Collections.Generic;
using System.Diagnostics;
using System.Formats.Asn1;
using System.Net;
using System.Security.Cryptography.Asn1;

namespace System.Security.Cryptography.X509Certificates
{
    /// <summary>
    /// Represents the Subject Alternative Name X.509 Extension (2.5.29.17).
    /// </summary>
    public sealed class X509SubjectAlternativeNameExtension : X509Extension
    {
        private List<GeneralNameAsn>? _decoded;

        /// <summary>
        /// Initializes a new instance of the <see cref="X509SubjectAlternativeNameExtension" /> class.
        /// </summary>
        public X509SubjectAlternativeNameExtension() : base(Oids.SubjectAltNameOid)
        {
            _decoded = new List<GeneralNameAsn>(0);
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="X509SubjectAlternativeNameExtension" /> class from an encoded representation of the extension and an optional critical marker.
        /// </summary>
        /// <param name="rawData">The encoded data to use to create the extension.</param>
        /// <param name="critical"><see langword="true" /> if the extension is critical; otherwise, <see langword="false" />. The default is <see langword="false" />.</param>
        public X509SubjectAlternativeNameExtension(byte[] rawData, bool critical = false)
            : base(Oids.SubjectAltNameOid, rawData, critical)
        {
            _decoded = Decode(RawData);
        }

        /// <summary>
        /// Initializes a new instance of the <see cref="X509SubjectAlternativeNameExtension" /> class from an encoded representation of the extension and an optional critical marker.
        /// </summary>
        /// <param name="rawData">The encoded data to use to create the extension.</param>
        /// <param name="critical"><see langword="true" /> if the extension is critical; otherwise, <see langword="false" />. The default is <see langword="false" />.</param>
        public X509SubjectAlternativeNameExtension(ReadOnlySpan<byte> rawData, bool critical = false)
            : base(Oids.SubjectAltNameOid, rawData, critical)
        {
            _decoded = Decode(RawData);
        }

        /// <summary>
        /// Copies the extension properties of the specified <see cref="AsnEncodedData" /> object.
        /// </summary>
        /// <param name="asnEncodedData">The <see cref="AsnEncodedData" /> object whose extension properties to copy.</param>
        public override void CopyFrom(AsnEncodedData asnEncodedData)
        {
            base.CopyFrom(asnEncodedData);
            _decoded = null;
        }

        /// <summary>
        /// Enumerates the alternative name entries with a DNS Name type identifier.
        /// </summary>
        /// <returns>An enumerable collection of alternative names with DNS Name type identifiers.</returns>
        public IEnumerable<string> EnumerateDnsNames()
        {
            List<GeneralNameAsn> decoded = (_decoded ??= Decode(RawData));

            return EnumerateDnsNames(decoded);
        }

        private static IEnumerable<string> EnumerateDnsNames(List<GeneralNameAsn> decoded)
        {
            foreach (GeneralNameAsn item in decoded)
            {
                if (item.DnsName is not null)
                {
                    yield return item.DnsName;
                }
            }
        }

        /// <summary>
        /// Enumerates the alternative name entries with an IP Address type identifier.
        /// </summary>
        /// <returns>An enumerable collection of alternative names with IP Address type identifiers.</returns>
        public IEnumerable<IPAddress> EnumerateIPAddresses()
        {
            List<GeneralNameAsn> decoded = (_decoded ??= Decode(RawData));

            return EnumerateIPAddresses(decoded);
        }

        private static IEnumerable<IPAddress> EnumerateIPAddresses(List<GeneralNameAsn> decoded)
        {
            foreach (GeneralNameAsn item in decoded)
            {
                if (item.IPAddress.HasValue)
                {
                    ReadOnlySpan<byte> value = item.IPAddress.GetValueOrDefault().Span;

                    Debug.Assert(value.Length is 4 or 16);
                    yield return new IPAddress(value);
                }
            }
        }

        private static List<GeneralNameAsn> Decode(ReadOnlySpan<byte> rawData)
        {
            try
            {
                AsnValueReader outer = new AsnValueReader(rawData, AsnEncodingRules.DER);
                AsnValueReader sequence = outer.ReadSequence();
                outer.ThrowIfNotEmpty();

                List<GeneralNameAsn> decoded = new List<GeneralNameAsn>();

                while (sequence.HasData)
                {
                    GeneralNameAsn.Decode(ref sequence, default, out GeneralNameAsn item);

                    // GeneralName already validates dNSName is a valid IA5String,
                    // so check iPAddress here so that it's always a consistent decode failure.
                    if (item.IPAddress.HasValue)
                    {
                        switch (item.IPAddress.GetValueOrDefault().Length)
                        {
                            case 4:
                                // IPv4
                            case 16:
                                // UPv6
                                break;
                            default:
                                throw new CryptographicException(
                                    SR.Cryptography_X509_SAN_UnknownIPAddressSize);
                        }
                    }

                    decoded.Add(item);
                }

                return decoded;
            }
            catch (AsnContentException e)
            {
                throw new CryptographicException(SR.Cryptography_Der_Invalid_Encoding, e);
            }
        }
    }
}
