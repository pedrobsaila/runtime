// Licensed to the .NET Foundation under one or more agreements.
// The .NET Foundation licenses this file to you under the MIT license.

using System;
using System.Collections.Specialized;
using System.IO;
using System.Text;

namespace System.Net.Mime
{
    /// <summary>
    /// Provides an abstraction for writing a MIME multi-part
    /// message.
    /// </summary>
    internal sealed class MimeWriter : BaseWriter
    {
        private readonly byte[] _boundaryBytes;
        private bool _writeBoundary = true;

        internal MimeWriter(Stream stream, string boundary)
            : base(stream, false) // Unnecessary, the underlying MailWriter stream already encodes dots
        {
            ArgumentNullException.ThrowIfNull(boundary);

            _boundaryBytes = Encoding.ASCII.GetBytes(boundary);
        }

        internal override void WriteHeaders(NameValueCollection headers, bool allowUnicode)
        {
            ArgumentNullException.ThrowIfNull(headers);

            foreach (string key in headers)
                WriteHeader(key, headers[key]!, allowUnicode);
        }

        #region Cleanup

        internal override void Close()
        {
            _bufferBuilder.Append("\r\n--"u8);
            _bufferBuilder.Append(_boundaryBytes);
            _bufferBuilder.Append("--\r\n"u8);
            Flush();
            _stream.Close();
        }

        /// <summary>
        /// Called when the current stream is closed.  Allows us to
        /// prepare for the next message part.
        /// </summary>
        /// <param name="sender">Sender of the close event</param>
        /// <param name="args">Event args (not used)</param>
        protected override void OnClose(object? sender, EventArgs args)
        {
            if (_contentStream != sender)
                return; // may have called WriteHeader

            _contentStream.Flush();
            _contentStream = null!;
            _writeBoundary = true;

            _isInContent = false;
        }

        #endregion Cleanup

        /// <summary>
        /// Writes the boundary sequence if required.
        /// </summary>
        protected override void CheckBoundary()
        {
            if (_writeBoundary)
            {
                _bufferBuilder.Append("\r\n--"u8);
                _bufferBuilder.Append(_boundaryBytes);
                _bufferBuilder.Append("\r\n"u8);
                _writeBoundary = false;
            }
        }
    }
}
