#ifndef REVC_CUSTOM_MINJSON_H
#define REVC_CUSTOM_MINJSON_H

// Tiny read-only JSON reader for the custom-data parsers.
//
// It covers the subset the client's files use: objects, arrays, numbers,
// strings, booleans, null, a UTF-8 BOM and (tolerantly) // and /* */ comments.
// The document is copied once so strtod() always sees a terminated buffer, and
// nesting is depth limited so a hostile or truncated file cannot exhaust the
// stack. Nothing here throws and nothing aborts: the caller gets false plus a
// short reason.

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

// The engine spells NULL as `nil` (core/common.h). These headers stay usable on
// their own in the host tests, so give the alias a local definition.
#ifndef nil
#define nil NULL
#endif

namespace minijson {

struct Value
{
	// Nested so callers can spell Value::ARR; the tag names stay the same as
	// the reader always used.
	enum Type { NIL, BOOL, NUM, STR, ARR, OBJ };

	Type type;
	bool b;
	double num;
	std::string str;
	std::vector<Value> items;              // ARR
	std::vector<std::string> keys;         // OBJ, parallel to members
	std::vector<Value> members;            // OBJ

	Value() : type(NIL), b(false), num(0.0) {}
	Value(const Value &other) { *this = other; }
	Value &operator=(const Value &other);

	// convenience aliases so a `Type` can also be named without the struct
	bool IsObject(void) const { return type == OBJ; }
	bool IsArray(void) const { return type == ARR; }
	bool IsNumber(void) const { return type == NUM; }

	const Value *Find(const char *key) const
	{
		if(type != OBJ || key == nil)
			return nil;
		for(size_t i = 0; i < keys.size(); i++)
			if(keys[i] == key)
				return &members[i];
		return nil;
	}
	size_t Count(void) const { return type == ARR ? items.size() : (type == OBJ ? members.size() : 0); }
};

// Deep copy without recursing through std::vector's element copies (which is
// exactly what the implicitly generated copy constructor would do anyway, this
// just keeps the intent explicit and lets us keep the class allocator-free).
inline Value &
Value::operator=(const Value &other)
{
	if(this == &other)
		return *this;
	type = other.type;
	b = other.b;
	num = other.num;
	str = other.str;
	items = other.items;
	keys = other.keys;
	members = other.members;
	return *this;
}

class Parser
{
public:
	Parser(const char *begin, const char *end) : p(begin), end(end), depth(0) {}

	std::string err;

	bool Run(Value &out)
	{
		if(!ParseValue(out))
			return false;
		SkipSpace();
		if(p != end) {
			err = "trailing characters after the document";
			return false;
		}
		return true;
	}

private:
	const char *p;
	const char *end;
	int depth;

	void SkipSpace(void)
	{
		for(;;) {
			while(p < end && (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n' ||
			                  *p == '\xef' || *p == '\xbb' || *p == '\xbf'))
				p++;
			if(p + 1 < end && p[0] == '/' && p[1] == '/') {
				while(p < end && *p != '\n')
					p++;
				continue;
			}
			if(p + 1 < end && p[0] == '/' && p[1] == '*') {
				p += 2;
				while(p + 1 < end && !(p[0] == '*' && p[1] == '/'))
					p++;
				p = (p + 1 < end) ? p + 2 : end;
				continue;
			}
			return;
		}
	}

	bool ParseValue(Value &out)
	{
		SkipSpace();
		if(p >= end) {
			err = "unexpected end of document";
			return false;
		}
		if(++depth > 64) {
			err = "nesting is too deep";
			return false;
		}
		bool ok = false;
		switch(*p) {
		case '{': ok = ParseObject(out); break;
		case '[': ok = ParseArray(out); break;
		case '"': out.type = Value::STR; ok = ParseString(out.str); break;
		case 't':
			if(end - p >= 4 && memcmp(p, "true", 4) == 0) {
				p += 4;
				out.type = Value::BOOL;
				out.b = true;
				ok = true;
			} else
				err = "bad literal";
			break;
		case 'f':
			if(end - p >= 5 && memcmp(p, "false", 5) == 0) {
				p += 5;
				out.type = Value::BOOL;
				out.b = false;
				ok = true;
			} else
				err = "bad literal";
			break;
		case 'n':
			if(end - p >= 4 && memcmp(p, "null", 4) == 0) {
				p += 4;
				out.type = Value::NIL;
				ok = true;
			} else
				err = "bad literal";
			break;
		default: {
			char *next = nil;
			double v = strtod(p, &next);
			if(next == p || next > end) {
				err = "bad number";
				break;
			}
			out.type = Value::NUM;
			out.num = v;
			p = next;
			ok = true;
			break;
		}
		}
		depth--;
		return ok;
	}

	bool ParseObject(Value &out)
	{
		out.type = Value::OBJ;
		p++;	// '{'
		SkipSpace();
		if(p < end && *p == '}') {
			p++;
			return true;
		}
		for(;;) {
			SkipSpace();
			if(p >= end || *p != '"') {
				err = "expected an object key";
				return false;
			}
			std::string key;
			if(!ParseString(key))
				return false;
			SkipSpace();
			if(p >= end || *p != ':') {
				err = "expected ':'";
				return false;
			}
			p++;
			Value v;
			if(!ParseValue(v))
				return false;
			out.keys.push_back(key);
			out.members.push_back(v);
			SkipSpace();
			if(p < end && *p == '}') {
				p++;
				return true;
			}
			if(p >= end || *p != ',') {
				err = "expected ',' or '}'";
				return false;
			}
			p++;
		}
	}

	bool ParseArray(Value &out)
	{
		out.type = Value::ARR;
		p++;	// '['
		SkipSpace();
		if(p < end && *p == ']') {
			p++;
			return true;
		}
		for(;;) {
			Value v;
			if(!ParseValue(v))
				return false;
			out.items.push_back(v);
			SkipSpace();
			if(p < end && *p == ']') {
				p++;
				return true;
			}
			if(p >= end || *p != ',') {
				err = "expected ',' or ']'";
				return false;
			}
			p++;
		}
	}

	static int HexDigit(char c)
	{
		if(c >= '0' && c <= '9')
			return c - '0';
		if(c >= 'a' && c <= 'f')
			return c - 'a' + 10;
		if(c >= 'A' && c <= 'F')
			return c - 'A' + 10;
		return -1;
	}

	bool ParseString(std::string &out)
	{
		p++;	// opening quote
		out.clear();
		while(p < end && *p != '"') {
			char c = *p;
			if(c == '\\' && p + 1 < end) {
				p++;
				switch(*p) {
				case 'n': out += '\n'; break;
				case 't': out += '\t'; break;
				case 'r': out += '\r'; break;
				case 'b': out += '\b'; break;
				case 'f': out += '\f'; break;
				case '/': out += '/'; break;
				case '"': out += '"'; break;
				case '\\': out += '\\'; break;
				case 'u': {
					if(end - p >= 5 && HexDigit(p[1]) >= 0 && HexDigit(p[2]) >= 0 &&
					   HexDigit(p[3]) >= 0 && HexDigit(p[4]) >= 0) {
						unsigned w = (unsigned)(HexDigit(p[1]) << 12 | HexDigit(p[2]) << 8 |
						                        HexDigit(p[3]) << 4 | HexDigit(p[4]));
						// A UTF-8 downcast is enough for the names in these files.
						if(w < 0x80)
							out += (char)w;
						else if(w < 0x800) {
							out += (char)(0xC0 | (w >> 6));
							out += (char)(0x80 | (w & 0x3F));
						} else {
							out += (char)(0xE0 | (w >> 12));
							out += (char)(0x80 | ((w >> 6) & 0x3F));
							out += (char)(0x80 | (w & 0x3F));
						}
						p += 4;
					}
					break;
				}
				default: out += *p; break;
				}
				p++;
			} else {
				out += c;
				p++;
			}
		}
		if(p >= end) {
			err = "unterminated string";
			return false;
		}
		p++;	// closing quote
		return true;
	}
};

// Parses one document. The buffer does not have to be terminated: it is copied
// once so strtod() is always safe.
inline bool
Parse(const char *data, size_t size, Value &out, std::string &error)
{
	error.clear();
	out = Value();
	if(data == nil && size != 0) {
		error = "null buffer";
		return false;
	}
	size_t off = 0;
	if(size >= 3 && (unsigned char)data[0] == 0xEF &&
	   (unsigned char)data[1] == 0xBB && (unsigned char)data[2] == 0xBF)
		off = 3;
	std::string copy(data == nil ? "" : data + off, size - off);
	Parser parser(copy.c_str(), copy.c_str() + copy.size());
	if(!parser.Run(out)) {
		error = parser.err;
		return false;
	}
	return true;
}

inline bool
Parse(const std::vector<unsigned char> &data, Value &out, std::string &error)
{
	if(data.empty())
		return Parse(nil, 0, out, error);
	return Parse((const char *)&data[0], data.size(), out, error);
}

} // namespace minijson

#endif // REVC_CUSTOM_MINJSON_H
