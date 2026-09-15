/* Scalar C17 decoder port from meshoptimizer v0.22. Encoders and SIMD removed;
 * C++ references/templates replaced with C pointers/specializations. */
/*
MIT License

Copyright (c) 2016-2024 Arseny Kapoulkine

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

*/
#define MR_MESHOPT_ASSERT(x) ((void)0)
static const unsigned char kVertexHeader=0xa0,kIndexHeader=0xe0,kSequenceHeader=0xd0;
#define kVertexBlockSizeBytes 8192
#define kVertexBlockMaxSize 256
#define kByteGroupSize 16
#define kByteGroupDecodeLimit 24
#define kTailMaxSize 32
typedef unsigned int VertexFifo[16];
typedef unsigned int EdgeFifo[16][2];
static size_t getVertexBlockSize(size_t vertex_size)
{
	// make sure the entire block fits into the scratch buffer
	size_t result = kVertexBlockSizeBytes / vertex_size;

	// align to byte group size; we encode each byte as a byte group
	// if vertex block is misaligned, it results in wasted bytes, so just truncate the block size
	result &= ~(kByteGroupSize - 1);

	return (result < kVertexBlockMaxSize) ? result : kVertexBlockMaxSize;
}
static unsigned char unzigzag8(unsigned char v)
{
	return -(v & 1) ^ (v >> 1);
}
static const unsigned char* decodeBytesGroup(const unsigned char* data, unsigned char* buffer, int bitslog2)
{
#define READ() byte = *data++
#define NEXT(bits) enc = byte >> (8 - bits), byte <<= bits, encv = *data_var, *buffer++ = (enc == (1 << bits) - 1) ? encv : enc, data_var += (enc == (1 << bits) - 1)

	unsigned char byte, enc, encv;
	const unsigned char* data_var;

	switch (bitslog2)
	{
	case 0:
		memset(buffer, 0, kByteGroupSize);
		return data;
	case 1:
		data_var = data + 4;

		// 4 groups with 4 2-bit values in each byte
		READ(), NEXT(2), NEXT(2), NEXT(2), NEXT(2);
		READ(), NEXT(2), NEXT(2), NEXT(2), NEXT(2);
		READ(), NEXT(2), NEXT(2), NEXT(2), NEXT(2);
		READ(), NEXT(2), NEXT(2), NEXT(2), NEXT(2);

		return data_var;
	case 2:
		data_var = data + 8;

		// 8 groups with 2 4-bit values in each byte
		READ(), NEXT(4), NEXT(4);
		READ(), NEXT(4), NEXT(4);
		READ(), NEXT(4), NEXT(4);
		READ(), NEXT(4), NEXT(4);
		READ(), NEXT(4), NEXT(4);
		READ(), NEXT(4), NEXT(4);
		READ(), NEXT(4), NEXT(4);
		READ(), NEXT(4), NEXT(4);

		return data_var;
	case 3:
		memcpy(buffer, data, kByteGroupSize);
		return data + kByteGroupSize;
	default:
		MR_MESHOPT_ASSERT(!"Unexpected bit length"); // unreachable since bitslog2 is a 2-bit value
		return data;
	}

#undef READ
#undef NEXT
}
static const unsigned char* decodeBytes(const unsigned char* data, const unsigned char* data_end, unsigned char* buffer, size_t buffer_size)
{
	MR_MESHOPT_ASSERT(buffer_size % kByteGroupSize == 0);

	const unsigned char* header = data;

	// round number of groups to 4 to get number of header bytes
	size_t header_size = (buffer_size / kByteGroupSize + 3) / 4;

	if (((size_t)(data_end - data)) < header_size)
		return NULL;

	data += header_size;

	for (size_t i = 0; i < buffer_size; i += kByteGroupSize)
	{
		if (((size_t)(data_end - data)) < kByteGroupDecodeLimit)
			return NULL;

		size_t header_offset = i / kByteGroupSize;

		int bitslog2 = (header[header_offset / 4] >> ((header_offset % 4) * 2)) & 3;

		data = decodeBytesGroup(data, buffer + i, bitslog2);
	}

	return data;
}
static const unsigned char* decodeVertexBlock(const unsigned char* data, const unsigned char* data_end, unsigned char* vertex_data, size_t vertex_count, size_t vertex_size, unsigned char last_vertex[256])
{
	MR_MESHOPT_ASSERT(vertex_count > 0 && vertex_count <= kVertexBlockMaxSize);

	unsigned char buffer[kVertexBlockMaxSize];
	unsigned char transposed[kVertexBlockSizeBytes];

	size_t vertex_count_aligned = (vertex_count + kByteGroupSize - 1) & ~(kByteGroupSize - 1);
	MR_MESHOPT_ASSERT(vertex_count <= vertex_count_aligned);

	for (size_t k = 0; k < vertex_size; ++k)
	{
		data = decodeBytes(data, data_end, buffer, vertex_count_aligned);
		if (!data)
			return NULL;

		size_t vertex_offset = k;

		unsigned char p = last_vertex[k];

		for (size_t i = 0; i < vertex_count; ++i)
		{
			unsigned char v = unzigzag8(buffer[i]) + p;

			transposed[vertex_offset] = v;
			p = v;

			vertex_offset += vertex_size;
		}
	}

	memcpy(vertex_data, transposed, vertex_count * vertex_size);

	memcpy(last_vertex, &transposed[vertex_size * (vertex_count - 1)], vertex_size);

	return data;
}
static int meshopt_decodeVertexBuffer(void* destination, size_t vertex_count, size_t vertex_size, const unsigned char* buffer, size_t buffer_size)
{

	MR_MESHOPT_ASSERT(vertex_size > 0 && vertex_size <= 256);
	MR_MESHOPT_ASSERT(vertex_size % 4 == 0);

	const unsigned char* (*decode)(const unsigned char*, const unsigned char*, unsigned char*, size_t, size_t, unsigned char[256]) = NULL;

decode = decodeVertexBlock;



	unsigned char* vertex_data = ((unsigned char*)(destination));

	const unsigned char* data = buffer;
	const unsigned char* data_end = buffer + buffer_size;

	if (((size_t)(data_end - data)) < 1 + vertex_size)
		return -2;

	unsigned char data_header = *data++;

	if ((data_header & 0xf0) != kVertexHeader)
		return -1;

	int version = data_header & 0x0f;
	if (version > 0)
		return -1;

	unsigned char last_vertex[256];
	memcpy(last_vertex, data_end - vertex_size, vertex_size);

	size_t vertex_block_size = getVertexBlockSize(vertex_size);

	size_t vertex_offset = 0;

	while (vertex_offset < vertex_count)
	{
		size_t block_size = (vertex_offset + vertex_block_size < vertex_count) ? vertex_block_size : vertex_count - vertex_offset;

		data = decode(data, data_end, vertex_data + vertex_offset * vertex_size, block_size, vertex_size, last_vertex);
		if (!data)
			return -2;

		vertex_offset += block_size;
	}

	size_t tail_size = vertex_size < kTailMaxSize ? kTailMaxSize : vertex_size;

	if (((size_t)(data_end - data)) != tail_size)
		return -3;

	return 0;
}
static void pushEdgeFifo(EdgeFifo fifo, unsigned int a, unsigned int b, size_t* offset)
{
	fifo[(*offset)][0] = a;
	fifo[(*offset)][1] = b;
	(*offset) = ((*offset) + 1) & 15;
}
static void pushVertexFifo(VertexFifo fifo, unsigned int v, size_t* offset, int cond)
{
	fifo[(*offset)] = v;
	(*offset) = ((*offset) + cond) & 15;
}
static unsigned int decodeVByte(const unsigned char** data)
{
	unsigned char lead = *(*data)++;

	// fast path: single byte
	if (lead < 128)
		return lead;

	// slow path: up to 4 extra bytes
	// note that this loop always terminates, which is important for malformed data
	unsigned int result = lead & 127;
	unsigned int shift = 7;

	for (int i = 0; i < 4; ++i)
	{
		unsigned char group = *(*data)++;
		result |= ((unsigned)(group & 127)) << shift;
		shift += 7;

		if (group < 128)
			break;
	}

	return result;
}
static unsigned int decodeIndex(const unsigned char** data, unsigned int last)
{
	unsigned int v = decodeVByte(data);
	unsigned int d = (v >> 1) ^ -((int)(v & 1));

	return last + d;
}
static void writeTriangle(void* destination, size_t offset, size_t index_size, unsigned int a, unsigned int b, unsigned int c)
{
	if (index_size == 2)
	{
		((unsigned short*)(destination))[offset + 0] = (unsigned short)(a);
		((unsigned short*)(destination))[offset + 1] = (unsigned short)(b);
		((unsigned short*)(destination))[offset + 2] = (unsigned short)(c);
	}
	else
	{
		((unsigned int*)(destination))[offset + 0] = a;
		((unsigned int*)(destination))[offset + 1] = b;
		((unsigned int*)(destination))[offset + 2] = c;
	}
}
static int meshopt_decodeIndexBuffer(void* destination, size_t index_count, size_t index_size, const unsigned char* buffer, size_t buffer_size)
{

	MR_MESHOPT_ASSERT(index_count % 3 == 0);
	MR_MESHOPT_ASSERT(index_size == 2 || index_size == 4);

	// the minimum valid encoding is header, 1 byte per triangle and a 16-byte codeaux table
	if (buffer_size < 1 + index_count / 3 + 16)
		return -2;

	if ((buffer[0] & 0xf0) != kIndexHeader)
		return -1;

	int version = buffer[0] & 0x0f;
	if (version > 1)
		return -1;

	EdgeFifo edgefifo;
	memset(edgefifo, -1, sizeof(edgefifo));

	VertexFifo vertexfifo;
	memset(vertexfifo, -1, sizeof(vertexfifo));

	size_t edgefifooffset = 0;
	size_t vertexfifooffset = 0;

	unsigned int next = 0;
	unsigned int last = 0;

	int fecmax = version >= 1 ? 13 : 15;

	// since we store 16-byte codeaux table at the end, triangle data has to begin before data_safe_end
	const unsigned char* code = buffer + 1;
	const unsigned char* data = code + index_count / 3;
	const unsigned char* data_safe_end = buffer + buffer_size - 16;

	const unsigned char* codeaux_table = data_safe_end;

	for (size_t i = 0; i < index_count; i += 3)
	{
		// make sure we have enough data to read for a triangle
		// each triangle reads at most 16 bytes of data: 1b for codeaux and 5b for each free index
		// after this we can be sure we can read without extra bounds checks
		if (data > data_safe_end)
			return -2;

		unsigned char codetri = *code++;

		if (codetri < 0xf0)
		{
			int fe = codetri >> 4;

			// fifo reads are wrapped around 16 entry buffer
			unsigned int a = edgefifo[(edgefifooffset - 1 - fe) & 15][0];
			unsigned int b = edgefifo[(edgefifooffset - 1 - fe) & 15][1];

			int fec = codetri & 15;

			// note: this is the most common path in the entire decoder
			// inside this if we try to stay branchless (by using cmov/etc.) since these aren't predictable
			if (fec < fecmax)
			{
				// fifo reads are wrapped around 16 entry buffer
				unsigned int cf = vertexfifo[(vertexfifooffset - 1 - fec) & 15];
				unsigned int c = (fec == 0) ? next : cf;

				int fec0 = fec == 0;
				next += fec0;

				// output triangle
				writeTriangle(destination, i, index_size, a, b, c);

				// push vertex/edge fifo must match the encoding step *exactly* otherwise the data will not be decoded correctly
				pushVertexFifo(vertexfifo, c, &vertexfifooffset, fec0);

				pushEdgeFifo(edgefifo, c, b, &edgefifooffset);
				pushEdgeFifo(edgefifo, a, c, &edgefifooffset);
			}
			else
			{
				unsigned int c = 0;

				// fec - (fec ^ 3) decodes 13, 14 into -1, 1
				// note that we need to update the last index since free indices are delta-encoded
				last = c = (fec != 15) ? last + (fec - (fec ^ 3)) : decodeIndex(&data, last);

				// output triangle
				writeTriangle(destination, i, index_size, a, b, c);

				// push vertex/edge fifo must match the encoding step *exactly* otherwise the data will not be decoded correctly
				pushVertexFifo(vertexfifo, c, &vertexfifooffset, 1);

				pushEdgeFifo(edgefifo, c, b, &edgefifooffset);
				pushEdgeFifo(edgefifo, a, c, &edgefifooffset);
			}
		}
		else
		{
			// fast path: read codeaux from the table
			if (codetri < 0xfe)
			{
				unsigned char codeaux = codeaux_table[codetri & 15];

				// note: table can't contain feb/fec=15
				int feb = codeaux >> 4;
				int fec = codeaux & 15;

				// fifo reads are wrapped around 16 entry buffer
				// also note that we increment next for all three vertices before decoding indices - this matches encoder behavior
				unsigned int a = next++;

				unsigned int bf = vertexfifo[(vertexfifooffset - feb) & 15];
				unsigned int b = (feb == 0) ? next : bf;

				int feb0 = feb == 0;
				next += feb0;

				unsigned int cf = vertexfifo[(vertexfifooffset - fec) & 15];
				unsigned int c = (fec == 0) ? next : cf;

				int fec0 = fec == 0;
				next += fec0;

				// output triangle
				writeTriangle(destination, i, index_size, a, b, c);

				// push vertex/edge fifo must match the encoding step *exactly* otherwise the data will not be decoded correctly
				pushVertexFifo(vertexfifo, a, &vertexfifooffset, 1);
				pushVertexFifo(vertexfifo, b, &vertexfifooffset, feb0);
				pushVertexFifo(vertexfifo, c, &vertexfifooffset, fec0);

				pushEdgeFifo(edgefifo, b, a, &edgefifooffset);
				pushEdgeFifo(edgefifo, c, b, &edgefifooffset);
				pushEdgeFifo(edgefifo, a, c, &edgefifooffset);
			}
			else
			{
				// slow path: read a full byte for codeaux instead of using a table lookup
				unsigned char codeaux = *data++;

				int fea = codetri == 0xfe ? 0 : 15;
				int feb = codeaux >> 4;
				int fec = codeaux & 15;

				// reset: codeaux is 0 but encoded as not-a-table
				if (codeaux == 0)
					next = 0;

				// fifo reads are wrapped around 16 entry buffer
				// also note that we increment next for all three vertices before decoding indices - this matches encoder behavior
				unsigned int a = (fea == 0) ? next++ : 0;
				unsigned int b = (feb == 0) ? next++ : vertexfifo[(vertexfifooffset - feb) & 15];
				unsigned int c = (fec == 0) ? next++ : vertexfifo[(vertexfifooffset - fec) & 15];

				// note that we need to update the last index since free indices are delta-encoded
				if (fea == 15)
					last = a = decodeIndex(&data, last);

				if (feb == 15)
					last = b = decodeIndex(&data, last);

				if (fec == 15)
					last = c = decodeIndex(&data, last);

				// output triangle
				writeTriangle(destination, i, index_size, a, b, c);

				// push vertex/edge fifo must match the encoding step *exactly* otherwise the data will not be decoded correctly
				pushVertexFifo(vertexfifo, a, &vertexfifooffset, 1);
				pushVertexFifo(vertexfifo, b, &vertexfifooffset, (feb == 0) | (feb == 15));
				pushVertexFifo(vertexfifo, c, &vertexfifooffset, (fec == 0) | (fec == 15));

				pushEdgeFifo(edgefifo, b, a, &edgefifooffset);
				pushEdgeFifo(edgefifo, c, b, &edgefifooffset);
				pushEdgeFifo(edgefifo, a, c, &edgefifooffset);
			}
		}
	}

	// we should've read all data bytes and stopped at the boundary between data and codeaux table
	if (data != data_safe_end)
		return -3;

	return 0;
}
static int meshopt_decodeIndexSequence(void* destination, size_t index_count, size_t index_size, const unsigned char* buffer, size_t buffer_size)
{

	// the minimum valid encoding is header, 1 byte per index and a 4-byte tail
	if (buffer_size < 1 + index_count + 4)
		return -2;

	if ((buffer[0] & 0xf0) != kSequenceHeader)
		return -1;

	int version = buffer[0] & 0x0f;
	if (version > 1)
		return -1;

	const unsigned char* data = buffer + 1;
	const unsigned char* data_safe_end = buffer + buffer_size - 4;

	unsigned int last[2] = {};

	for (size_t i = 0; i < index_count; ++i)
	{
		// make sure we have enough data to read
		// each index reads at most 5 bytes of data; there's a 4 byte tail after data_safe_end
		// after this we can be sure we can read without extra bounds checks
		if (data >= data_safe_end)
			return -2;

		unsigned int v = decodeVByte(&data);

		// decode the index of the last baseline
		unsigned int current = v & 1;
		v >>= 1;

		// reconstruct index as a delta
		unsigned int d = (v >> 1) ^ -((int)(v & 1));
		unsigned int index = last[current] + d;

		// update last for the next iteration that uses it
		last[current] = index;

		if (index_size == 2)
		{
			((unsigned short*)(destination))[i] = (unsigned short)(index);
		}
		else
		{
			((unsigned int*)(destination))[i] = index;
		}
	}

	// we should've read all data bytes and stopped at the boundary between data and tail
	if (data != data_safe_end)
		return -3;

	return 0;
}
static void mr_meshopt_oct8(signed char* data, size_t count)
{
	const float max = (float)((1 << (sizeof(signed char) * 8 - 1)) - 1);

	for (size_t i = 0; i < count; ++i)
	{
		// convert x and y to floats and reconstruct z; this assumes zf encodes 1.f at the same bit count
		float x = ((float)(data[i * 4 + 0]));
		float y = ((float)(data[i * 4 + 1]));
		float z = ((float)(data[i * 4 + 2])) - __builtin_fabsf(x) - __builtin_fabsf(y);

		// fixup octahedral coordinates for z<0
		float t = (z >= 0.f) ? 0.f : z;

		x += (x >= 0.f) ? t : -t;
		y += (y >= 0.f) ? t : -t;

		// compute normal length & scale
		float l = sqrtf(x * x + y * y + z * z);
		float s = max / l;

		// rounded signed float->int
		int xf = (int)(x * s + (x >= 0.f ? 0.5f : -0.5f));
		int yf = (int)(y * s + (y >= 0.f ? 0.5f : -0.5f));
		int zf = (int)(z * s + (z >= 0.f ? 0.5f : -0.5f));

		data[i * 4 + 0] = ((signed char)(xf));
		data[i * 4 + 1] = ((signed char)(yf));
		data[i * 4 + 2] = ((signed char)(zf));
	}
}
static void mr_meshopt_oct16(short* data, size_t count)
{
	const float max = (float)((1 << (sizeof(short) * 8 - 1)) - 1);

	for (size_t i = 0; i < count; ++i)
	{
		// convert x and y to floats and reconstruct z; this assumes zf encodes 1.f at the same bit count
		float x = ((float)(data[i * 4 + 0]));
		float y = ((float)(data[i * 4 + 1]));
		float z = ((float)(data[i * 4 + 2])) - __builtin_fabsf(x) - __builtin_fabsf(y);

		// fixup octahedral coordinates for z<0
		float t = (z >= 0.f) ? 0.f : z;

		x += (x >= 0.f) ? t : -t;
		y += (y >= 0.f) ? t : -t;

		// compute normal length & scale
		float l = sqrtf(x * x + y * y + z * z);
		float s = max / l;

		// rounded signed float->int
		int xf = (int)(x * s + (x >= 0.f ? 0.5f : -0.5f));
		int yf = (int)(y * s + (y >= 0.f ? 0.5f : -0.5f));
		int zf = (int)(z * s + (z >= 0.f ? 0.5f : -0.5f));

		data[i * 4 + 0] = ((short)(xf));
		data[i * 4 + 1] = ((short)(yf));
		data[i * 4 + 2] = ((short)(zf));
	}
}
static void decodeFilterQuat(short* data, size_t count)
{
	const float scale = 1.f / sqrtf(2.f);

	for (size_t i = 0; i < count; ++i)
	{
		// recover scale from the high byte of the component
		int sf = data[i * 4 + 3] | 3;
		float ss = scale / ((float)(sf));

		// convert x/y/z to [-1..1] (scaled...)
		float x = ((float)(data[i * 4 + 0])) * ss;
		float y = ((float)(data[i * 4 + 1])) * ss;
		float z = ((float)(data[i * 4 + 2])) * ss;

		// reconstruct w as a square root; we clamp to 0.f to avoid NaN due to precision errors
		float ww = 1.f - x * x - y * y - z * z;
		float w = sqrtf(ww >= 0.f ? ww : 0.f);

		// rounded signed float->int
		int xf = (int)(x * 32767.f + (x >= 0.f ? 0.5f : -0.5f));
		int yf = (int)(y * 32767.f + (y >= 0.f ? 0.5f : -0.5f));
		int zf = (int)(z * 32767.f + (z >= 0.f ? 0.5f : -0.5f));
		int wf = ((int)(w * 32767.f + 0.5f));

		int qc = data[i * 4 + 3] & 3;

		// output order is dictated by input index
		data[i * 4 + ((qc + 1) & 3)] = ((short)(xf));
		data[i * 4 + ((qc + 2) & 3)] = ((short)(yf));
		data[i * 4 + ((qc + 3) & 3)] = ((short)(zf));
		data[i * 4 + ((qc + 0) & 3)] = ((short)(wf));
	}
}
static void decodeFilterExp(unsigned int* data, size_t count)
{
	for (size_t i = 0; i < count; ++i)
	{
		unsigned int v = data[i];

		// decode mantissa and exponent
		int m = ((int)(v << 8)) >> 8;
		int e = ((int)(v)) >> 24;

		union
		{
			float f;
			unsigned int ui;
		} u;

		// optimized version of ldexp(((float)(m)), e)
		u.ui = ((unsigned)(e + 127)) << 23;
		u.f = u.f * ((float)(m));

		data[i] = u.ui;
	}
}

#undef MR_MESHOPT_ASSERT
#undef kVertexBlockSizeBytes
#undef kVertexBlockMaxSize
#undef kByteGroupSize
#undef kByteGroupDecodeLimit
#undef kTailMaxSize
