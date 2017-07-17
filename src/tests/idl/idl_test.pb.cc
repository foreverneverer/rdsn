// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: idl_test.proto

#define INTERNAL_SUPPRESS_PROTOBUF_FIELD_DEPRECATION
#include "idl_test.pb.h"

#include <algorithm>

#include <google/protobuf/stubs/common.h>
#include <google/protobuf/stubs/port.h>
#include <google/protobuf/stubs/once.h>
#include <google/protobuf/io/coded_stream.h>
#include <google/protobuf/wire_format_lite_inl.h>
#include <google/protobuf/descriptor.h>
#include <google/protobuf/generated_message_reflection.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/wire_format.h>
// @@protoc_insertion_point(includes)

namespace dsn {
namespace idl {
namespace test {

namespace {

const ::google::protobuf::Descriptor *test_protobuf_item_descriptor_ = NULL;
const ::google::protobuf::internal::GeneratedMessageReflection *test_protobuf_item_reflection_ =
    NULL;
const ::google::protobuf::Descriptor *test_protobuf_item_MapInt32ItemEntry_descriptor_ = NULL;

} // namespace

void protobuf_AssignDesc_idl_5ftest_2eproto()
{
    protobuf_AddDesc_idl_5ftest_2eproto();
    const ::google::protobuf::FileDescriptor *file =
        ::google::protobuf::DescriptorPool::generated_pool()->FindFileByName("idl_test.proto");
    GOOGLE_CHECK(file != NULL);
    test_protobuf_item_descriptor_ = file->message_type(0);
    static const int test_protobuf_item_offsets_[10] = {
        GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(test_protobuf_item, bool_item_),
        GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(test_protobuf_item, int32_item_),
        GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(test_protobuf_item, int64_item_),
        GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(test_protobuf_item, uint32_item_),
        GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(test_protobuf_item, uint64_item_),
        GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(test_protobuf_item, float_item_),
        GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(test_protobuf_item, double_item_),
        GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(test_protobuf_item, string_item_),
        GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(test_protobuf_item, map_int32_item_),
        GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(test_protobuf_item, repeated_int32_item_),
    };
    test_protobuf_item_reflection_ =
        ::google::protobuf::internal::GeneratedMessageReflection::NewGeneratedMessageReflection(
            test_protobuf_item_descriptor_,
            test_protobuf_item::default_instance_,
            test_protobuf_item_offsets_,
            -1,
            -1,
            -1,
            sizeof(test_protobuf_item),
            GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(test_protobuf_item, _internal_metadata_),
            GOOGLE_PROTOBUF_GENERATED_MESSAGE_FIELD_OFFSET(test_protobuf_item,
                                                           _is_default_instance_));
    test_protobuf_item_MapInt32ItemEntry_descriptor_ =
        test_protobuf_item_descriptor_->nested_type(0);
}

namespace {

GOOGLE_PROTOBUF_DECLARE_ONCE(protobuf_AssignDescriptors_once_);
inline void protobuf_AssignDescriptorsOnce()
{
    ::google::protobuf::GoogleOnceInit(&protobuf_AssignDescriptors_once_,
                                       &protobuf_AssignDesc_idl_5ftest_2eproto);
}

void protobuf_RegisterTypes(const ::std::string &)
{
    protobuf_AssignDescriptorsOnce();
    ::google::protobuf::MessageFactory::InternalRegisterGeneratedMessage(
        test_protobuf_item_descriptor_, &test_protobuf_item::default_instance());
    ::google::protobuf::MessageFactory::InternalRegisterGeneratedMessage(
        test_protobuf_item_MapInt32ItemEntry_descriptor_,
        ::google::protobuf::internal::MapEntry<
            ::google::protobuf::int32,
            ::google::protobuf::int32,
            ::google::protobuf::internal::WireFormatLite::TYPE_INT32,
            ::google::protobuf::internal::WireFormatLite::TYPE_INT32,
            0>::CreateDefaultInstance(test_protobuf_item_MapInt32ItemEntry_descriptor_));
}

} // namespace

void protobuf_ShutdownFile_idl_5ftest_2eproto()
{
    delete test_protobuf_item::default_instance_;
    delete test_protobuf_item_reflection_;
}

void protobuf_AddDesc_idl_5ftest_2eproto()
{
    static bool already_here = false;
    if (already_here)
        return;
    already_here = true;
    GOOGLE_PROTOBUF_VERIFY_VERSION;

    ::google::protobuf::DescriptorPool::InternalAddGeneratedFile(
        "\n\016idl_test.proto\022\014dsn.idl.test\"\325\002\n\022test_"
        "protobuf_item\022\021\n\tbool_item\030\001 \001(\010\022\022\n\nint3"
        "2_item\030\002 \001(\005\022\022\n\nint64_item\030\003 \001(\003\022\023\n\013uint"
        "32_item\030\004 \001(\r\022\023\n\013uint64_item\030\005 \001(\004\022\022\n\nfl"
        "oat_item\030\006 \001(\002\022\023\n\013double_item\030\007 \001(\001\022\023\n\013s"
        "tring_item\030\010 \001(\t\022J\n\016map_int32_item\030\t \003(\013"
        "22.dsn.idl.test.test_protobuf_item.MapIn"
        "t32ItemEntry\022\033\n\023repeated_int32_item\030\n \003("
        "\005\0323\n\021MapInt32ItemEntry\022\013\n\003key\030\001 \001(\005\022\r\n\005v"
        "alue\030\002 \001(\005:\0028\001b\006proto3",
        382);
    ::google::protobuf::MessageFactory::InternalRegisterGeneratedFile("idl_test.proto",
                                                                      &protobuf_RegisterTypes);
    test_protobuf_item::default_instance_ = new test_protobuf_item();
    test_protobuf_item::default_instance_->InitAsDefaultInstance();
    ::google::protobuf::internal::OnShutdown(&protobuf_ShutdownFile_idl_5ftest_2eproto);
}

// Force AddDescriptors() to be called at static initialization time.
struct StaticDescriptorInitializer_idl_5ftest_2eproto
{
    StaticDescriptorInitializer_idl_5ftest_2eproto() { protobuf_AddDesc_idl_5ftest_2eproto(); }
} static_descriptor_initializer_idl_5ftest_2eproto_;

namespace {

static void MergeFromFail(int line) GOOGLE_ATTRIBUTE_COLD;
static void MergeFromFail(int line) { GOOGLE_CHECK(false) << __FILE__ << ":" << line; }

} // namespace

// ===================================================================

#if !defined(_MSC_VER) || _MSC_VER >= 1900
const int test_protobuf_item::kBoolItemFieldNumber;
const int test_protobuf_item::kInt32ItemFieldNumber;
const int test_protobuf_item::kInt64ItemFieldNumber;
const int test_protobuf_item::kUint32ItemFieldNumber;
const int test_protobuf_item::kUint64ItemFieldNumber;
const int test_protobuf_item::kFloatItemFieldNumber;
const int test_protobuf_item::kDoubleItemFieldNumber;
const int test_protobuf_item::kStringItemFieldNumber;
const int test_protobuf_item::kMapInt32ItemFieldNumber;
const int test_protobuf_item::kRepeatedInt32ItemFieldNumber;
#endif // !defined(_MSC_VER) || _MSC_VER >= 1900

test_protobuf_item::test_protobuf_item() : ::google::protobuf::Message(), _internal_metadata_(NULL)
{
    SharedCtor();
    // @@protoc_insertion_point(constructor:dsn.idl.test.test_protobuf_item)
}

void test_protobuf_item::InitAsDefaultInstance() { _is_default_instance_ = true; }

test_protobuf_item::test_protobuf_item(const test_protobuf_item &from)
    : ::google::protobuf::Message(), _internal_metadata_(NULL)
{
    SharedCtor();
    MergeFrom(from);
    // @@protoc_insertion_point(copy_constructor:dsn.idl.test.test_protobuf_item)
}

void test_protobuf_item::SharedCtor()
{
    _is_default_instance_ = false;
    ::google::protobuf::internal::GetEmptyString();
    _cached_size_ = 0;
    bool_item_ = false;
    int32_item_ = 0;
    int64_item_ = GOOGLE_LONGLONG(0);
    uint32_item_ = 0u;
    uint64_item_ = GOOGLE_ULONGLONG(0);
    float_item_ = 0;
    double_item_ = 0;
    string_item_.UnsafeSetDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
    map_int32_item_.SetAssignDescriptorCallback(protobuf_AssignDescriptorsOnce);
    map_int32_item_.SetEntryDescriptor(
        &::dsn::idl::test::test_protobuf_item_MapInt32ItemEntry_descriptor_);
}

test_protobuf_item::~test_protobuf_item()
{
    // @@protoc_insertion_point(destructor:dsn.idl.test.test_protobuf_item)
    SharedDtor();
}

void test_protobuf_item::SharedDtor()
{
    string_item_.DestroyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
    if (this != default_instance_) {
    }
}

void test_protobuf_item::SetCachedSize(int size) const
{
    GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
    _cached_size_ = size;
    GOOGLE_SAFE_CONCURRENT_WRITES_END();
}
const ::google::protobuf::Descriptor *test_protobuf_item::descriptor()
{
    protobuf_AssignDescriptorsOnce();
    return test_protobuf_item_descriptor_;
}

const test_protobuf_item &test_protobuf_item::default_instance()
{
    if (default_instance_ == NULL)
        protobuf_AddDesc_idl_5ftest_2eproto();
    return *default_instance_;
}

test_protobuf_item *test_protobuf_item::default_instance_ = NULL;

test_protobuf_item *test_protobuf_item::New(::google::protobuf::Arena *arena) const
{
    test_protobuf_item *n = new test_protobuf_item;
    if (arena != NULL) {
        arena->Own(n);
    }
    return n;
}

void test_protobuf_item::Clear()
{
#define ZR_HELPER_(f) reinterpret_cast<char *>(&reinterpret_cast<test_protobuf_item *>(16)->f)

#define ZR_(first, last)                                                                           \
    do {                                                                                           \
        ::memset(&first, 0, ZR_HELPER_(last) - ZR_HELPER_(first) + sizeof(last));                  \
    } while (0)

    ZR_(bool_item_, double_item_);
    string_item_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());

#undef ZR_HELPER_
#undef ZR_

    map_int32_item_.Clear();
    repeated_int32_item_.Clear();
}

bool test_protobuf_item::MergePartialFromCodedStream(
    ::google::protobuf::io::CodedInputStream *input)
{
#define DO_(EXPRESSION)                                                                            \
    if (!(EXPRESSION))                                                                             \
    goto failure
    ::google::protobuf::uint32 tag;
    // @@protoc_insertion_point(parse_start:dsn.idl.test.test_protobuf_item)
    for (;;) {
        ::std::pair<::google::protobuf::uint32, bool> p = input->ReadTagWithCutoff(127);
        tag = p.first;
        if (!p.second)
            goto handle_unusual;
        switch (::google::protobuf::internal::WireFormatLite::GetTagFieldNumber(tag)) {
        // optional bool bool_item = 1;
        case 1: {
            if (tag == 8) {
                DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                     bool,
                     ::google::protobuf::internal::WireFormatLite::TYPE_BOOL>(input, &bool_item_)));

            } else {
                goto handle_unusual;
            }
            if (input->ExpectTag(16))
                goto parse_int32_item;
            break;
        }

        // optional int32 int32_item = 2;
        case 2: {
            if (tag == 16) {
            parse_int32_item:
                DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                     ::google::protobuf::int32,
                     ::google::protobuf::internal::WireFormatLite::TYPE_INT32>(input,
                                                                               &int32_item_)));

            } else {
                goto handle_unusual;
            }
            if (input->ExpectTag(24))
                goto parse_int64_item;
            break;
        }

        // optional int64 int64_item = 3;
        case 3: {
            if (tag == 24) {
            parse_int64_item:
                DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                     ::google::protobuf::int64,
                     ::google::protobuf::internal::WireFormatLite::TYPE_INT64>(input,
                                                                               &int64_item_)));

            } else {
                goto handle_unusual;
            }
            if (input->ExpectTag(32))
                goto parse_uint32_item;
            break;
        }

        // optional uint32 uint32_item = 4;
        case 4: {
            if (tag == 32) {
            parse_uint32_item:
                DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                     ::google::protobuf::uint32,
                     ::google::protobuf::internal::WireFormatLite::TYPE_UINT32>(input,
                                                                                &uint32_item_)));

            } else {
                goto handle_unusual;
            }
            if (input->ExpectTag(40))
                goto parse_uint64_item;
            break;
        }

        // optional uint64 uint64_item = 5;
        case 5: {
            if (tag == 40) {
            parse_uint64_item:
                DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                     ::google::protobuf::uint64,
                     ::google::protobuf::internal::WireFormatLite::TYPE_UINT64>(input,
                                                                                &uint64_item_)));

            } else {
                goto handle_unusual;
            }
            if (input->ExpectTag(53))
                goto parse_float_item;
            break;
        }

        // optional float float_item = 6;
        case 6: {
            if (tag == 53) {
            parse_float_item:
                DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                     float,
                     ::google::protobuf::internal::WireFormatLite::TYPE_FLOAT>(input,
                                                                               &float_item_)));

            } else {
                goto handle_unusual;
            }
            if (input->ExpectTag(57))
                goto parse_double_item;
            break;
        }

        // optional double double_item = 7;
        case 7: {
            if (tag == 57) {
            parse_double_item:
                DO_((::google::protobuf::internal::WireFormatLite::ReadPrimitive<
                     double,
                     ::google::protobuf::internal::WireFormatLite::TYPE_DOUBLE>(input,
                                                                                &double_item_)));

            } else {
                goto handle_unusual;
            }
            if (input->ExpectTag(66))
                goto parse_string_item;
            break;
        }

        // optional string string_item = 8;
        case 8: {
            if (tag == 66) {
            parse_string_item:
                DO_(::google::protobuf::internal::WireFormatLite::ReadString(
                    input, this->mutable_string_item()));
                DO_(::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
                    this->string_item().data(),
                    this->string_item().length(),
                    ::google::protobuf::internal::WireFormatLite::PARSE,
                    "dsn.idl.test.test_protobuf_item.string_item"));
            } else {
                goto handle_unusual;
            }
            if (input->ExpectTag(74))
                goto parse_map_int32_item;
            break;
        }

        // map<int32, int32> map_int32_item = 9;
        case 9: {
            if (tag == 74) {
            parse_map_int32_item:
                DO_(input->IncrementRecursionDepth());
            parse_loop_map_int32_item:
                ::google::protobuf::scoped_ptr<test_protobuf_item_MapInt32ItemEntry> entry(
                    map_int32_item_.NewEntry());
                DO_(::google::protobuf::internal::WireFormatLite::ReadMessageNoVirtual(
                    input, entry.get()));
                (*mutable_map_int32_item())[entry->key()] = *entry->mutable_value();
            } else {
                goto handle_unusual;
            }
            if (input->ExpectTag(74))
                goto parse_loop_map_int32_item;
            input->UnsafeDecrementRecursionDepth();
            if (input->ExpectTag(82))
                goto parse_repeated_int32_item;
            break;
        }

        // repeated int32 repeated_int32_item = 10;
        case 10: {
            if (tag == 82) {
            parse_repeated_int32_item:
                DO_((::google::protobuf::internal::WireFormatLite::ReadPackedPrimitive<
                     ::google::protobuf::int32,
                     ::google::protobuf::internal::WireFormatLite::TYPE_INT32>(
                    input, this->mutable_repeated_int32_item())));
            } else if (tag == 80) {
                DO_((::google::protobuf::internal::WireFormatLite::ReadRepeatedPrimitiveNoInline<
                     ::google::protobuf::int32,
                     ::google::protobuf::internal::WireFormatLite::TYPE_INT32>(
                    1, 82, input, this->mutable_repeated_int32_item())));
            } else {
                goto handle_unusual;
            }
            if (input->ExpectAtEnd())
                goto success;
            break;
        }

        default: {
        handle_unusual:
            if (tag == 0 ||
                ::google::protobuf::internal::WireFormatLite::GetTagWireType(tag) ==
                    ::google::protobuf::internal::WireFormatLite::WIRETYPE_END_GROUP) {
                goto success;
            }
            DO_(::google::protobuf::internal::WireFormatLite::SkipField(input, tag));
            break;
        }
        }
    }
success:
    // @@protoc_insertion_point(parse_success:dsn.idl.test.test_protobuf_item)
    return true;
failure:
    // @@protoc_insertion_point(parse_failure:dsn.idl.test.test_protobuf_item)
    return false;
#undef DO_
}

void test_protobuf_item::SerializeWithCachedSizes(
    ::google::protobuf::io::CodedOutputStream *output) const
{
    // @@protoc_insertion_point(serialize_start:dsn.idl.test.test_protobuf_item)
    // optional bool bool_item = 1;
    if (this->bool_item() != 0) {
        ::google::protobuf::internal::WireFormatLite::WriteBool(1, this->bool_item(), output);
    }

    // optional int32 int32_item = 2;
    if (this->int32_item() != 0) {
        ::google::protobuf::internal::WireFormatLite::WriteInt32(2, this->int32_item(), output);
    }

    // optional int64 int64_item = 3;
    if (this->int64_item() != 0) {
        ::google::protobuf::internal::WireFormatLite::WriteInt64(3, this->int64_item(), output);
    }

    // optional uint32 uint32_item = 4;
    if (this->uint32_item() != 0) {
        ::google::protobuf::internal::WireFormatLite::WriteUInt32(4, this->uint32_item(), output);
    }

    // optional uint64 uint64_item = 5;
    if (this->uint64_item() != 0) {
        ::google::protobuf::internal::WireFormatLite::WriteUInt64(5, this->uint64_item(), output);
    }

    // optional float float_item = 6;
    if (this->float_item() != 0) {
        ::google::protobuf::internal::WireFormatLite::WriteFloat(6, this->float_item(), output);
    }

    // optional double double_item = 7;
    if (this->double_item() != 0) {
        ::google::protobuf::internal::WireFormatLite::WriteDouble(7, this->double_item(), output);
    }

    // optional string string_item = 8;
    if (this->string_item().size() > 0) {
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            this->string_item().data(),
            this->string_item().length(),
            ::google::protobuf::internal::WireFormatLite::SERIALIZE,
            "dsn.idl.test.test_protobuf_item.string_item");
        ::google::protobuf::internal::WireFormatLite::WriteStringMaybeAliased(
            8, this->string_item(), output);
    }

    // map<int32, int32> map_int32_item = 9;
    {
        ::google::protobuf::scoped_ptr<test_protobuf_item_MapInt32ItemEntry> entry;
        for (::google::protobuf::Map<::google::protobuf::int32,
                                     ::google::protobuf::int32>::const_iterator it =
                 this->map_int32_item().begin();
             it != this->map_int32_item().end();
             ++it) {
            entry.reset(map_int32_item_.NewEntryWrapper(it->first, it->second));
            ::google::protobuf::internal::WireFormatLite::WriteMessageMaybeToArray(
                9, *entry, output);
        }
    }

    // repeated int32 repeated_int32_item = 10;
    if (this->repeated_int32_item_size() > 0) {
        ::google::protobuf::internal::WireFormatLite::WriteTag(
            10, ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED, output);
        output->WriteVarint32(_repeated_int32_item_cached_byte_size_);
    }
    for (int i = 0; i < this->repeated_int32_item_size(); i++) {
        ::google::protobuf::internal::WireFormatLite::WriteInt32NoTag(this->repeated_int32_item(i),
                                                                      output);
    }

    // @@protoc_insertion_point(serialize_end:dsn.idl.test.test_protobuf_item)
}

::google::protobuf::uint8 *
test_protobuf_item::SerializeWithCachedSizesToArray(::google::protobuf::uint8 *target) const
{
    // @@protoc_insertion_point(serialize_to_array_start:dsn.idl.test.test_protobuf_item)
    // optional bool bool_item = 1;
    if (this->bool_item() != 0) {
        target = ::google::protobuf::internal::WireFormatLite::WriteBoolToArray(
            1, this->bool_item(), target);
    }

    // optional int32 int32_item = 2;
    if (this->int32_item() != 0) {
        target = ::google::protobuf::internal::WireFormatLite::WriteInt32ToArray(
            2, this->int32_item(), target);
    }

    // optional int64 int64_item = 3;
    if (this->int64_item() != 0) {
        target = ::google::protobuf::internal::WireFormatLite::WriteInt64ToArray(
            3, this->int64_item(), target);
    }

    // optional uint32 uint32_item = 4;
    if (this->uint32_item() != 0) {
        target = ::google::protobuf::internal::WireFormatLite::WriteUInt32ToArray(
            4, this->uint32_item(), target);
    }

    // optional uint64 uint64_item = 5;
    if (this->uint64_item() != 0) {
        target = ::google::protobuf::internal::WireFormatLite::WriteUInt64ToArray(
            5, this->uint64_item(), target);
    }

    // optional float float_item = 6;
    if (this->float_item() != 0) {
        target = ::google::protobuf::internal::WireFormatLite::WriteFloatToArray(
            6, this->float_item(), target);
    }

    // optional double double_item = 7;
    if (this->double_item() != 0) {
        target = ::google::protobuf::internal::WireFormatLite::WriteDoubleToArray(
            7, this->double_item(), target);
    }

    // optional string string_item = 8;
    if (this->string_item().size() > 0) {
        ::google::protobuf::internal::WireFormatLite::VerifyUtf8String(
            this->string_item().data(),
            this->string_item().length(),
            ::google::protobuf::internal::WireFormatLite::SERIALIZE,
            "dsn.idl.test.test_protobuf_item.string_item");
        target = ::google::protobuf::internal::WireFormatLite::WriteStringToArray(
            8, this->string_item(), target);
    }

    // map<int32, int32> map_int32_item = 9;
    {
        ::google::protobuf::scoped_ptr<test_protobuf_item_MapInt32ItemEntry> entry;
        for (::google::protobuf::Map<::google::protobuf::int32,
                                     ::google::protobuf::int32>::const_iterator it =
                 this->map_int32_item().begin();
             it != this->map_int32_item().end();
             ++it) {
            entry.reset(map_int32_item_.NewEntryWrapper(it->first, it->second));
            target = ::google::protobuf::internal::WireFormatLite::WriteMessageNoVirtualToArray(
                9, *entry, target);
        }
    }

    // repeated int32 repeated_int32_item = 10;
    if (this->repeated_int32_item_size() > 0) {
        target = ::google::protobuf::internal::WireFormatLite::WriteTagToArray(
            10, ::google::protobuf::internal::WireFormatLite::WIRETYPE_LENGTH_DELIMITED, target);
        target = ::google::protobuf::io::CodedOutputStream::WriteVarint32ToArray(
            _repeated_int32_item_cached_byte_size_, target);
    }
    for (int i = 0; i < this->repeated_int32_item_size(); i++) {
        target = ::google::protobuf::internal::WireFormatLite::WriteInt32NoTagToArray(
            this->repeated_int32_item(i), target);
    }

    // @@protoc_insertion_point(serialize_to_array_end:dsn.idl.test.test_protobuf_item)
    return target;
}

int test_protobuf_item::ByteSize() const
{
    int total_size = 0;

    // optional bool bool_item = 1;
    if (this->bool_item() != 0) {
        total_size += 1 + 1;
    }

    // optional int32 int32_item = 2;
    if (this->int32_item() != 0) {
        total_size +=
            1 + ::google::protobuf::internal::WireFormatLite::Int32Size(this->int32_item());
    }

    // optional int64 int64_item = 3;
    if (this->int64_item() != 0) {
        total_size +=
            1 + ::google::protobuf::internal::WireFormatLite::Int64Size(this->int64_item());
    }

    // optional uint32 uint32_item = 4;
    if (this->uint32_item() != 0) {
        total_size +=
            1 + ::google::protobuf::internal::WireFormatLite::UInt32Size(this->uint32_item());
    }

    // optional uint64 uint64_item = 5;
    if (this->uint64_item() != 0) {
        total_size +=
            1 + ::google::protobuf::internal::WireFormatLite::UInt64Size(this->uint64_item());
    }

    // optional float float_item = 6;
    if (this->float_item() != 0) {
        total_size += 1 + 4;
    }

    // optional double double_item = 7;
    if (this->double_item() != 0) {
        total_size += 1 + 8;
    }

    // optional string string_item = 8;
    if (this->string_item().size() > 0) {
        total_size +=
            1 + ::google::protobuf::internal::WireFormatLite::StringSize(this->string_item());
    }

    // map<int32, int32> map_int32_item = 9;
    total_size += 1 * this->map_int32_item_size();
    {
        ::google::protobuf::scoped_ptr<test_protobuf_item_MapInt32ItemEntry> entry;
        for (::google::protobuf::Map<::google::protobuf::int32,
                                     ::google::protobuf::int32>::const_iterator it =
                 this->map_int32_item().begin();
             it != this->map_int32_item().end();
             ++it) {
            entry.reset(map_int32_item_.NewEntryWrapper(it->first, it->second));
            total_size +=
                ::google::protobuf::internal::WireFormatLite::MessageSizeNoVirtual(*entry);
        }
    }

    // repeated int32 repeated_int32_item = 10;
    {
        int data_size = 0;
        for (int i = 0; i < this->repeated_int32_item_size(); i++) {
            data_size += ::google::protobuf::internal::WireFormatLite::Int32Size(
                this->repeated_int32_item(i));
        }
        if (data_size > 0) {
            total_size += 1 + ::google::protobuf::internal::WireFormatLite::Int32Size(data_size);
        }
        GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
        _repeated_int32_item_cached_byte_size_ = data_size;
        GOOGLE_SAFE_CONCURRENT_WRITES_END();
        total_size += data_size;
    }

    GOOGLE_SAFE_CONCURRENT_WRITES_BEGIN();
    _cached_size_ = total_size;
    GOOGLE_SAFE_CONCURRENT_WRITES_END();
    return total_size;
}

void test_protobuf_item::MergeFrom(const ::google::protobuf::Message &from)
{
    if (GOOGLE_PREDICT_FALSE(&from == this))
        MergeFromFail(__LINE__);
    const test_protobuf_item *source =
        ::google::protobuf::internal::DynamicCastToGenerated<const test_protobuf_item>(&from);
    if (source == NULL) {
        ::google::protobuf::internal::ReflectionOps::Merge(from, this);
    } else {
        MergeFrom(*source);
    }
}

void test_protobuf_item::MergeFrom(const test_protobuf_item &from)
{
    if (GOOGLE_PREDICT_FALSE(&from == this))
        MergeFromFail(__LINE__);
    map_int32_item_.MergeFrom(from.map_int32_item_);
    repeated_int32_item_.MergeFrom(from.repeated_int32_item_);
    if (from.bool_item() != 0) {
        set_bool_item(from.bool_item());
    }
    if (from.int32_item() != 0) {
        set_int32_item(from.int32_item());
    }
    if (from.int64_item() != 0) {
        set_int64_item(from.int64_item());
    }
    if (from.uint32_item() != 0) {
        set_uint32_item(from.uint32_item());
    }
    if (from.uint64_item() != 0) {
        set_uint64_item(from.uint64_item());
    }
    if (from.float_item() != 0) {
        set_float_item(from.float_item());
    }
    if (from.double_item() != 0) {
        set_double_item(from.double_item());
    }
    if (from.string_item().size() > 0) {

        string_item_.AssignWithDefault(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
                                       from.string_item_);
    }
}

void test_protobuf_item::CopyFrom(const ::google::protobuf::Message &from)
{
    if (&from == this)
        return;
    Clear();
    MergeFrom(from);
}

void test_protobuf_item::CopyFrom(const test_protobuf_item &from)
{
    if (&from == this)
        return;
    Clear();
    MergeFrom(from);
}

bool test_protobuf_item::IsInitialized() const { return true; }

void test_protobuf_item::Swap(test_protobuf_item *other)
{
    if (other == this)
        return;
    InternalSwap(other);
}
void test_protobuf_item::InternalSwap(test_protobuf_item *other)
{
    std::swap(bool_item_, other->bool_item_);
    std::swap(int32_item_, other->int32_item_);
    std::swap(int64_item_, other->int64_item_);
    std::swap(uint32_item_, other->uint32_item_);
    std::swap(uint64_item_, other->uint64_item_);
    std::swap(float_item_, other->float_item_);
    std::swap(double_item_, other->double_item_);
    string_item_.Swap(&other->string_item_);
    map_int32_item_.Swap(&other->map_int32_item_);
    repeated_int32_item_.UnsafeArenaSwap(&other->repeated_int32_item_);
    _internal_metadata_.Swap(&other->_internal_metadata_);
    std::swap(_cached_size_, other->_cached_size_);
}

::google::protobuf::Metadata test_protobuf_item::GetMetadata() const
{
    protobuf_AssignDescriptorsOnce();
    ::google::protobuf::Metadata metadata;
    metadata.descriptor = test_protobuf_item_descriptor_;
    metadata.reflection = test_protobuf_item_reflection_;
    return metadata;
}

#if PROTOBUF_INLINE_NOT_IN_HEADERS
// test_protobuf_item

// optional bool bool_item = 1;
void test_protobuf_item::clear_bool_item() { bool_item_ = false; }
bool test_protobuf_item::bool_item() const
{
    // @@protoc_insertion_point(field_get:dsn.idl.test.test_protobuf_item.bool_item)
    return bool_item_;
}
void test_protobuf_item::set_bool_item(bool value)
{

    bool_item_ = value;
    // @@protoc_insertion_point(field_set:dsn.idl.test.test_protobuf_item.bool_item)
}

// optional int32 int32_item = 2;
void test_protobuf_item::clear_int32_item() { int32_item_ = 0; }
::google::protobuf::int32 test_protobuf_item::int32_item() const
{
    // @@protoc_insertion_point(field_get:dsn.idl.test.test_protobuf_item.int32_item)
    return int32_item_;
}
void test_protobuf_item::set_int32_item(::google::protobuf::int32 value)
{

    int32_item_ = value;
    // @@protoc_insertion_point(field_set:dsn.idl.test.test_protobuf_item.int32_item)
}

// optional int64 int64_item = 3;
void test_protobuf_item::clear_int64_item() { int64_item_ = GOOGLE_LONGLONG(0); }
::google::protobuf::int64 test_protobuf_item::int64_item() const
{
    // @@protoc_insertion_point(field_get:dsn.idl.test.test_protobuf_item.int64_item)
    return int64_item_;
}
void test_protobuf_item::set_int64_item(::google::protobuf::int64 value)
{

    int64_item_ = value;
    // @@protoc_insertion_point(field_set:dsn.idl.test.test_protobuf_item.int64_item)
}

// optional uint32 uint32_item = 4;
void test_protobuf_item::clear_uint32_item() { uint32_item_ = 0u; }
::google::protobuf::uint32 test_protobuf_item::uint32_item() const
{
    // @@protoc_insertion_point(field_get:dsn.idl.test.test_protobuf_item.uint32_item)
    return uint32_item_;
}
void test_protobuf_item::set_uint32_item(::google::protobuf::uint32 value)
{

    uint32_item_ = value;
    // @@protoc_insertion_point(field_set:dsn.idl.test.test_protobuf_item.uint32_item)
}

// optional uint64 uint64_item = 5;
void test_protobuf_item::clear_uint64_item() { uint64_item_ = GOOGLE_ULONGLONG(0); }
::google::protobuf::uint64 test_protobuf_item::uint64_item() const
{
    // @@protoc_insertion_point(field_get:dsn.idl.test.test_protobuf_item.uint64_item)
    return uint64_item_;
}
void test_protobuf_item::set_uint64_item(::google::protobuf::uint64 value)
{

    uint64_item_ = value;
    // @@protoc_insertion_point(field_set:dsn.idl.test.test_protobuf_item.uint64_item)
}

// optional float float_item = 6;
void test_protobuf_item::clear_float_item() { float_item_ = 0; }
float test_protobuf_item::float_item() const
{
    // @@protoc_insertion_point(field_get:dsn.idl.test.test_protobuf_item.float_item)
    return float_item_;
}
void test_protobuf_item::set_float_item(float value)
{

    float_item_ = value;
    // @@protoc_insertion_point(field_set:dsn.idl.test.test_protobuf_item.float_item)
}

// optional double double_item = 7;
void test_protobuf_item::clear_double_item() { double_item_ = 0; }
double test_protobuf_item::double_item() const
{
    // @@protoc_insertion_point(field_get:dsn.idl.test.test_protobuf_item.double_item)
    return double_item_;
}
void test_protobuf_item::set_double_item(double value)
{

    double_item_ = value;
    // @@protoc_insertion_point(field_set:dsn.idl.test.test_protobuf_item.double_item)
}

// optional string string_item = 8;
void test_protobuf_item::clear_string_item()
{
    string_item_.ClearToEmptyNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
const ::std::string &test_protobuf_item::string_item() const
{
    // @@protoc_insertion_point(field_get:dsn.idl.test.test_protobuf_item.string_item)
    return string_item_.GetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
void test_protobuf_item::set_string_item(const ::std::string &value)
{

    string_item_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(), value);
    // @@protoc_insertion_point(field_set:dsn.idl.test.test_protobuf_item.string_item)
}
void test_protobuf_item::set_string_item(const char *value)
{

    string_item_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
                            ::std::string(value));
    // @@protoc_insertion_point(field_set_char:dsn.idl.test.test_protobuf_item.string_item)
}
void test_protobuf_item::set_string_item(const char *value, size_t size)
{

    string_item_.SetNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
                            ::std::string(reinterpret_cast<const char *>(value), size));
    // @@protoc_insertion_point(field_set_pointer:dsn.idl.test.test_protobuf_item.string_item)
}
::std::string *test_protobuf_item::mutable_string_item()
{

    // @@protoc_insertion_point(field_mutable:dsn.idl.test.test_protobuf_item.string_item)
    return string_item_.MutableNoArena(
        &::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
::std::string *test_protobuf_item::release_string_item()
{

    return string_item_.ReleaseNoArena(
        &::google::protobuf::internal::GetEmptyStringAlreadyInited());
}
void test_protobuf_item::set_allocated_string_item(::std::string *string_item)
{
    if (string_item != NULL) {

    } else {
    }
    string_item_.SetAllocatedNoArena(&::google::protobuf::internal::GetEmptyStringAlreadyInited(),
                                     string_item);
    // @@protoc_insertion_point(field_set_allocated:dsn.idl.test.test_protobuf_item.string_item)
}

// map<int32, int32> map_int32_item = 9;
int test_protobuf_item::map_int32_item_size() const { return map_int32_item_.size(); }
void test_protobuf_item::clear_map_int32_item() { map_int32_item_.Clear(); }
const ::google::protobuf::Map<::google::protobuf::int32, ::google::protobuf::int32> &
test_protobuf_item::map_int32_item() const
{
    // @@protoc_insertion_point(field_map:dsn.idl.test.test_protobuf_item.map_int32_item)
    return map_int32_item_.GetMap();
}
::google::protobuf::Map<::google::protobuf::int32, ::google::protobuf::int32> *
test_protobuf_item::mutable_map_int32_item()
{
    // @@protoc_insertion_point(field_mutable_map:dsn.idl.test.test_protobuf_item.map_int32_item)
    return map_int32_item_.MutableMap();
}

// repeated int32 repeated_int32_item = 10;
int test_protobuf_item::repeated_int32_item_size() const { return repeated_int32_item_.size(); }
void test_protobuf_item::clear_repeated_int32_item() { repeated_int32_item_.Clear(); }
::google::protobuf::int32 test_protobuf_item::repeated_int32_item(int index) const
{
    // @@protoc_insertion_point(field_get:dsn.idl.test.test_protobuf_item.repeated_int32_item)
    return repeated_int32_item_.Get(index);
}
void test_protobuf_item::set_repeated_int32_item(int index, ::google::protobuf::int32 value)
{
    repeated_int32_item_.Set(index, value);
    // @@protoc_insertion_point(field_set:dsn.idl.test.test_protobuf_item.repeated_int32_item)
}
void test_protobuf_item::add_repeated_int32_item(::google::protobuf::int32 value)
{
    repeated_int32_item_.Add(value);
    // @@protoc_insertion_point(field_add:dsn.idl.test.test_protobuf_item.repeated_int32_item)
}
const ::google::protobuf::RepeatedField<::google::protobuf::int32> &
test_protobuf_item::repeated_int32_item() const
{
    // @@protoc_insertion_point(field_list:dsn.idl.test.test_protobuf_item.repeated_int32_item)
    return repeated_int32_item_;
}
::google::protobuf::RepeatedField<::google::protobuf::int32> *
test_protobuf_item::mutable_repeated_int32_item()
{
    // @@protoc_insertion_point(field_mutable_list:dsn.idl.test.test_protobuf_item.repeated_int32_item)
    return &repeated_int32_item_;
}

#endif // PROTOBUF_INLINE_NOT_IN_HEADERS

// @@protoc_insertion_point(namespace_scope)

} // namespace test
} // namespace idl
} // namespace dsn

// @@protoc_insertion_point(global_scope)
