// Generated by the protocol buffer compiler.  DO NOT EDIT!
// source: proto/TgBotDB.proto
// Protobuf C++ Version: 5.26.1

#include "proto/TgBotDB.pb.h"

#include <algorithm>
#include "google/protobuf/io/coded_stream.h"
#include "google/protobuf/extension_set.h"
#include "google/protobuf/wire_format_lite.h"
#include "google/protobuf/descriptor.h"
#include "google/protobuf/generated_message_reflection.h"
#include "google/protobuf/reflection_ops.h"
#include "google/protobuf/wire_format.h"
#include "google/protobuf/generated_message_tctable_impl.h"
// @@protoc_insertion_point(includes)

// Must be included last.
#include "google/protobuf/port_def.inc"
PROTOBUF_PRAGMA_INIT_SEG
namespace _pb = ::google::protobuf;
namespace _pbi = ::google::protobuf::internal;
namespace _fl = ::google::protobuf::internal::field_layout;
namespace tgbot {
namespace proto {

inline constexpr PersonList::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : id_{},
        _cached_size_{0} {}

template <typename>
PROTOBUF_CONSTEXPR PersonList::PersonList(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct PersonListDefaultTypeInternal {
  PROTOBUF_CONSTEXPR PersonListDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~PersonListDefaultTypeInternal() {}
  union {
    PersonList _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 PersonListDefaultTypeInternal _PersonList_default_instance_;

inline constexpr MediaToName::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        names_{},
        telegrammediauniqueid_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()),
        telegrammediaid_(
            &::google::protobuf::internal::fixed_address_empty_string,
            ::_pbi::ConstantInitialized()) {}

template <typename>
PROTOBUF_CONSTEXPR MediaToName::MediaToName(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct MediaToNameDefaultTypeInternal {
  PROTOBUF_CONSTEXPR MediaToNameDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~MediaToNameDefaultTypeInternal() {}
  union {
    MediaToName _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 MediaToNameDefaultTypeInternal _MediaToName_default_instance_;

inline constexpr Database::Impl_::Impl_(
    ::_pbi::ConstantInitialized) noexcept
      : _cached_size_{0},
        mediatonames_{},
        whitelist_{nullptr},
        blacklist_{nullptr},
        ownerid_{::int64_t{0}} {}

template <typename>
PROTOBUF_CONSTEXPR Database::Database(::_pbi::ConstantInitialized)
    : _impl_(::_pbi::ConstantInitialized()) {}
struct DatabaseDefaultTypeInternal {
  PROTOBUF_CONSTEXPR DatabaseDefaultTypeInternal() : _instance(::_pbi::ConstantInitialized{}) {}
  ~DatabaseDefaultTypeInternal() {}
  union {
    Database _instance;
  };
};

PROTOBUF_ATTRIBUTE_NO_DESTROY PROTOBUF_CONSTINIT
    PROTOBUF_ATTRIBUTE_INIT_PRIORITY1 DatabaseDefaultTypeInternal _Database_default_instance_;
}  // namespace proto
}  // namespace tgbot
static ::_pb::Metadata file_level_metadata_proto_2fTgBotDB_2eproto[3];
static constexpr const ::_pb::EnumDescriptor**
    file_level_enum_descriptors_proto_2fTgBotDB_2eproto = nullptr;
static constexpr const ::_pb::ServiceDescriptor**
    file_level_service_descriptors_proto_2fTgBotDB_2eproto = nullptr;
const ::uint32_t
    TableStruct_proto_2fTgBotDB_2eproto::offsets[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
        protodesc_cold) = {
        ~0u,  // no _has_bits_
        PROTOBUF_FIELD_OFFSET(::tgbot::proto::PersonList, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::tgbot::proto::PersonList, _impl_.id_),
        PROTOBUF_FIELD_OFFSET(::tgbot::proto::MediaToName, _impl_._has_bits_),
        PROTOBUF_FIELD_OFFSET(::tgbot::proto::MediaToName, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::tgbot::proto::MediaToName, _impl_.telegrammediauniqueid_),
        PROTOBUF_FIELD_OFFSET(::tgbot::proto::MediaToName, _impl_.telegrammediaid_),
        PROTOBUF_FIELD_OFFSET(::tgbot::proto::MediaToName, _impl_.names_),
        0,
        1,
        ~0u,
        PROTOBUF_FIELD_OFFSET(::tgbot::proto::Database, _impl_._has_bits_),
        PROTOBUF_FIELD_OFFSET(::tgbot::proto::Database, _internal_metadata_),
        ~0u,  // no _extensions_
        ~0u,  // no _oneof_case_
        ~0u,  // no _weak_field_map_
        ~0u,  // no _inlined_string_donated_
        ~0u,  // no _split_
        ~0u,  // no sizeof(Split)
        PROTOBUF_FIELD_OFFSET(::tgbot::proto::Database, _impl_.ownerid_),
        PROTOBUF_FIELD_OFFSET(::tgbot::proto::Database, _impl_.whitelist_),
        PROTOBUF_FIELD_OFFSET(::tgbot::proto::Database, _impl_.blacklist_),
        PROTOBUF_FIELD_OFFSET(::tgbot::proto::Database, _impl_.mediatonames_),
        2,
        0,
        1,
        ~0u,
};

static const ::_pbi::MigrationSchema
    schemas[] ABSL_ATTRIBUTE_SECTION_VARIABLE(protodesc_cold) = {
        {0, -1, -1, sizeof(::tgbot::proto::PersonList)},
        {9, 20, -1, sizeof(::tgbot::proto::MediaToName)},
        {23, 35, -1, sizeof(::tgbot::proto::Database)},
};
static const ::_pb::Message* const file_default_instances[] = {
    &::tgbot::proto::_PersonList_default_instance_._instance,
    &::tgbot::proto::_MediaToName_default_instance_._instance,
    &::tgbot::proto::_Database_default_instance_._instance,
};
const char descriptor_table_protodef_proto_2fTgBotDB_2eproto[] ABSL_ATTRIBUTE_SECTION_VARIABLE(
    protodesc_cold) = {
    "\n\023proto/TgBotDB.proto\022\013tgbot.proto\"\030\n\nPe"
    "rsonList\022\n\n\002id\030\001 \003(\003\"T\n\013MediaToName\022\035\n\025T"
    "elegramMediaUniqueId\030\001 \001(\t\022\027\n\017TelegramMe"
    "diaId\030\002 \001(\t\022\r\n\005Names\030\003 \003(\t\"\243\001\n\010Database\022"
    "\017\n\007ownerId\030\001 \001(\003\022*\n\twhitelist\030\002 \001(\0132\027.tg"
    "bot.proto.PersonList\022*\n\tblacklist\030\003 \001(\0132"
    "\027.tgbot.proto.PersonList\022.\n\014mediaToNames"
    "\030\004 \003(\0132\030.tgbot.proto.MediaToName"
};
static ::absl::once_flag descriptor_table_proto_2fTgBotDB_2eproto_once;
const ::_pbi::DescriptorTable descriptor_table_proto_2fTgBotDB_2eproto = {
    false,
    false,
    312,
    descriptor_table_protodef_proto_2fTgBotDB_2eproto,
    "proto/TgBotDB.proto",
    &descriptor_table_proto_2fTgBotDB_2eproto_once,
    nullptr,
    0,
    3,
    schemas,
    file_default_instances,
    TableStruct_proto_2fTgBotDB_2eproto::offsets,
    file_level_metadata_proto_2fTgBotDB_2eproto,
    file_level_enum_descriptors_proto_2fTgBotDB_2eproto,
    file_level_service_descriptors_proto_2fTgBotDB_2eproto,
};

// This function exists to be marked as weak.
// It can significantly speed up compilation by breaking up LLVM's SCC
// in the .pb.cc translation units. Large translation units see a
// reduction of more than 35% of walltime for optimized builds. Without
// the weak attribute all the messages in the file, including all the
// vtables and everything they use become part of the same SCC through
// a cycle like:
// GetMetadata -> descriptor table -> default instances ->
//   vtables -> GetMetadata
// By adding a weak function here we break the connection from the
// individual vtables back into the descriptor table.
PROTOBUF_ATTRIBUTE_WEAK const ::_pbi::DescriptorTable* descriptor_table_proto_2fTgBotDB_2eproto_getter() {
  return &descriptor_table_proto_2fTgBotDB_2eproto;
}
namespace tgbot {
namespace proto {
// ===================================================================

class PersonList::_Internal {
 public:
};

PersonList::PersonList(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:tgbot.proto.PersonList)
}
inline PROTOBUF_NDEBUG_INLINE PersonList::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : id_{visibility, arena, from.id_},
        _cached_size_{0} {}

PersonList::PersonList(
    ::google::protobuf::Arena* arena,
    const PersonList& from)
    : ::google::protobuf::Message(arena) {
  PersonList* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);

  // @@protoc_insertion_point(copy_constructor:tgbot.proto.PersonList)
}
inline PROTOBUF_NDEBUG_INLINE PersonList::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : id_{visibility, arena},
        _cached_size_{0} {}

inline void PersonList::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
PersonList::~PersonList() {
  // @@protoc_insertion_point(destructor:tgbot.proto.PersonList)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void PersonList::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
PersonList::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(PersonList, _impl_._cached_size_),
              false,
          },
          &PersonList::MergeImpl,
          &PersonList::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void PersonList::Clear() {
// @@protoc_insertion_point(message_clear_start:tgbot.proto.PersonList)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.id_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* PersonList::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<0, 1, 0, 0, 2> PersonList::_table_ = {
  {
    0,  // no _has_bits_
    0, // no _extensions_
    1, 0,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967294,  // skipmap
    offsetof(decltype(_table_), field_entries),
    1,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    &_PersonList_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::tgbot::proto::PersonList>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated int64 id = 1;
    {::_pbi::TcParser::FastV64R1,
     {8, 63, 0, PROTOBUF_FIELD_OFFSET(PersonList, _impl_.id_)}},
  }}, {{
    65535, 65535
  }}, {{
    // repeated int64 id = 1;
    {PROTOBUF_FIELD_OFFSET(PersonList, _impl_.id_), 0, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kInt64)},
  }},
  // no aux_entries
  {{
  }},
};

::uint8_t* PersonList::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:tgbot.proto.PersonList)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  // repeated int64 id = 1;
  for (int i = 0, n = this->_internal_id_size(); i < n; ++i) {
    target = stream->EnsureSpace(target);
    target = ::_pbi::WireFormatLite::WriteInt64ToArray(
        1, this->_internal_id().Get(i), target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:tgbot.proto.PersonList)
  return target;
}

::size_t PersonList::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:tgbot.proto.PersonList)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // repeated int64 id = 1;
  {
    std::size_t data_size = ::_pbi::WireFormatLite::Int64Size(
        this->_internal_id())
    ;
    std::size_t tag_size = std::size_t{1} *
        ::_pbi::FromIntSize(this->_internal_id_size());
    ;
    total_size += tag_size + data_size;
  }
  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void PersonList::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<PersonList*>(&to_msg);
  auto& from = static_cast<const PersonList&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:tgbot.proto.PersonList)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_id()->MergeFrom(from._internal_id());
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void PersonList::CopyFrom(const PersonList& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:tgbot.proto.PersonList)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool PersonList::IsInitialized() const {
  return true;
}

void PersonList::InternalSwap(PersonList* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  _impl_.id_.InternalSwap(&other->_impl_.id_);
}

::google::protobuf::Metadata PersonList::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2fTgBotDB_2eproto_getter,
                                   &descriptor_table_proto_2fTgBotDB_2eproto_once,
                                   file_level_metadata_proto_2fTgBotDB_2eproto[0]);
}
// ===================================================================

class MediaToName::_Internal {
 public:
  using HasBits = decltype(std::declval<MediaToName>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
    8 * PROTOBUF_FIELD_OFFSET(MediaToName, _impl_._has_bits_);
};

MediaToName::MediaToName(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:tgbot.proto.MediaToName)
}
inline PROTOBUF_NDEBUG_INLINE MediaToName::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : _has_bits_{from._has_bits_},
        _cached_size_{0},
        names_{visibility, arena, from.names_},
        telegrammediauniqueid_(arena, from.telegrammediauniqueid_),
        telegrammediaid_(arena, from.telegrammediaid_) {}

MediaToName::MediaToName(
    ::google::protobuf::Arena* arena,
    const MediaToName& from)
    : ::google::protobuf::Message(arena) {
  MediaToName* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);

  // @@protoc_insertion_point(copy_constructor:tgbot.proto.MediaToName)
}
inline PROTOBUF_NDEBUG_INLINE MediaToName::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0},
        names_{visibility, arena},
        telegrammediauniqueid_(arena),
        telegrammediaid_(arena) {}

inline void MediaToName::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
}
MediaToName::~MediaToName() {
  // @@protoc_insertion_point(destructor:tgbot.proto.MediaToName)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void MediaToName::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  _impl_.telegrammediauniqueid_.Destroy();
  _impl_.telegrammediaid_.Destroy();
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
MediaToName::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(MediaToName, _impl_._cached_size_),
              false,
          },
          &MediaToName::MergeImpl,
          &MediaToName::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void MediaToName::Clear() {
// @@protoc_insertion_point(message_clear_start:tgbot.proto.MediaToName)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.names_.Clear();
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000003u) {
    if (cached_has_bits & 0x00000001u) {
      _impl_.telegrammediauniqueid_.ClearNonDefaultToEmpty();
    }
    if (cached_has_bits & 0x00000002u) {
      _impl_.telegrammediaid_.ClearNonDefaultToEmpty();
    }
  }
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* MediaToName::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 3, 0, 73, 2> MediaToName::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(MediaToName, _impl_._has_bits_),
    0, // no _extensions_
    3, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967288,  // skipmap
    offsetof(decltype(_table_), field_entries),
    3,  // num_field_entries
    0,  // num_aux_entries
    offsetof(decltype(_table_), field_names),  // no aux_entries
    &_MediaToName_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::tgbot::proto::MediaToName>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    {::_pbi::TcParser::MiniParse, {}},
    // optional string TelegramMediaUniqueId = 1;
    {::_pbi::TcParser::FastSS1,
     {10, 0, 0, PROTOBUF_FIELD_OFFSET(MediaToName, _impl_.telegrammediauniqueid_)}},
    // optional string TelegramMediaId = 2;
    {::_pbi::TcParser::FastSS1,
     {18, 1, 0, PROTOBUF_FIELD_OFFSET(MediaToName, _impl_.telegrammediaid_)}},
    // repeated string Names = 3;
    {::_pbi::TcParser::FastSR1,
     {26, 63, 0, PROTOBUF_FIELD_OFFSET(MediaToName, _impl_.names_)}},
  }}, {{
    65535, 65535
  }}, {{
    // optional string TelegramMediaUniqueId = 1;
    {PROTOBUF_FIELD_OFFSET(MediaToName, _impl_.telegrammediauniqueid_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kRawString | ::_fl::kRepAString)},
    // optional string TelegramMediaId = 2;
    {PROTOBUF_FIELD_OFFSET(MediaToName, _impl_.telegrammediaid_), _Internal::kHasBitsOffset + 1, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kRawString | ::_fl::kRepAString)},
    // repeated string Names = 3;
    {PROTOBUF_FIELD_OFFSET(MediaToName, _impl_.names_), -1, 0,
    (0 | ::_fl::kFcRepeated | ::_fl::kRawString | ::_fl::kRepSString)},
  }},
  // no aux_entries
  {{
    "\27\25\17\5\0\0\0\0"
    "tgbot.proto.MediaToName"
    "TelegramMediaUniqueId"
    "TelegramMediaId"
    "Names"
  }},
};

::uint8_t* MediaToName::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:tgbot.proto.MediaToName)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  // optional string TelegramMediaUniqueId = 1;
  if (cached_has_bits & 0x00000001u) {
    const std::string& _s = this->_internal_telegrammediauniqueid();
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(_s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormat::SERIALIZE,
                                "tgbot.proto.MediaToName.TelegramMediaUniqueId");
    target = stream->WriteStringMaybeAliased(1, _s, target);
  }

  // optional string TelegramMediaId = 2;
  if (cached_has_bits & 0x00000002u) {
    const std::string& _s = this->_internal_telegrammediaid();
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(_s.data(), static_cast<int>(_s.length()), ::google::protobuf::internal::WireFormat::SERIALIZE,
                                "tgbot.proto.MediaToName.TelegramMediaId");
    target = stream->WriteStringMaybeAliased(2, _s, target);
  }

  // repeated string Names = 3;
  for (int i = 0, n = this->_internal_names_size(); i < n; ++i) {
    const auto& s = this->_internal_names().Get(i);
    ::google::protobuf::internal::WireFormat::VerifyUTF8StringNamedField(s.data(), static_cast<int>(s.length()), ::google::protobuf::internal::WireFormat::SERIALIZE,
                                "tgbot.proto.MediaToName.Names");
    target = stream->WriteString(3, s, target);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:tgbot.proto.MediaToName)
  return target;
}

::size_t MediaToName::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:tgbot.proto.MediaToName)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // repeated string Names = 3;
  total_size += 1 * ::google::protobuf::internal::FromIntSize(_internal_names().size());
  for (int i = 0, n = _internal_names().size(); i < n; ++i) {
    total_size += ::google::protobuf::internal::WireFormatLite::StringSize(
        _internal_names().Get(i));
  }
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000003u) {
    // optional string TelegramMediaUniqueId = 1;
    if (cached_has_bits & 0x00000001u) {
      total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                      this->_internal_telegrammediauniqueid());
    }

    // optional string TelegramMediaId = 2;
    if (cached_has_bits & 0x00000002u) {
      total_size += 1 + ::google::protobuf::internal::WireFormatLite::StringSize(
                                      this->_internal_telegrammediaid());
    }

  }
  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void MediaToName::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<MediaToName*>(&to_msg);
  auto& from = static_cast<const MediaToName&>(from_msg);
  // @@protoc_insertion_point(class_specific_merge_from_start:tgbot.proto.MediaToName)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_names()->MergeFrom(from._internal_names());
  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000003u) {
    if (cached_has_bits & 0x00000001u) {
      _this->_internal_set_telegrammediauniqueid(from._internal_telegrammediauniqueid());
    }
    if (cached_has_bits & 0x00000002u) {
      _this->_internal_set_telegrammediaid(from._internal_telegrammediaid());
    }
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void MediaToName::CopyFrom(const MediaToName& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:tgbot.proto.MediaToName)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool MediaToName::IsInitialized() const {
  return true;
}

void MediaToName::InternalSwap(MediaToName* PROTOBUF_RESTRICT other) {
  using std::swap;
  auto* arena = GetArena();
  ABSL_DCHECK_EQ(arena, other->GetArena());
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  _impl_.names_.InternalSwap(&other->_impl_.names_);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.telegrammediauniqueid_, &other->_impl_.telegrammediauniqueid_, arena);
  ::_pbi::ArenaStringPtr::InternalSwap(&_impl_.telegrammediaid_, &other->_impl_.telegrammediaid_, arena);
}

::google::protobuf::Metadata MediaToName::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2fTgBotDB_2eproto_getter,
                                   &descriptor_table_proto_2fTgBotDB_2eproto_once,
                                   file_level_metadata_proto_2fTgBotDB_2eproto[1]);
}
// ===================================================================

class Database::_Internal {
 public:
  using HasBits = decltype(std::declval<Database>()._impl_._has_bits_);
  static constexpr ::int32_t kHasBitsOffset =
    8 * PROTOBUF_FIELD_OFFSET(Database, _impl_._has_bits_);
};

Database::Database(::google::protobuf::Arena* arena)
    : ::google::protobuf::Message(arena) {
  SharedCtor(arena);
  // @@protoc_insertion_point(arena_constructor:tgbot.proto.Database)
}
inline PROTOBUF_NDEBUG_INLINE Database::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility, ::google::protobuf::Arena* arena,
    const Impl_& from)
      : _has_bits_{from._has_bits_},
        _cached_size_{0},
        mediatonames_{visibility, arena, from.mediatonames_} {}

Database::Database(
    ::google::protobuf::Arena* arena,
    const Database& from)
    : ::google::protobuf::Message(arena) {
  Database* const _this = this;
  (void)_this;
  _internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(
      from._internal_metadata_);
  new (&_impl_) Impl_(internal_visibility(), arena, from._impl_);
  ::uint32_t cached_has_bits = _impl_._has_bits_[0];
  _impl_.whitelist_ = (cached_has_bits & 0x00000001u) ? ::google::protobuf::Message::CopyConstruct<::tgbot::proto::PersonList>(
                              arena, *from._impl_.whitelist_)
                        : nullptr;
  _impl_.blacklist_ = (cached_has_bits & 0x00000002u) ? ::google::protobuf::Message::CopyConstruct<::tgbot::proto::PersonList>(
                              arena, *from._impl_.blacklist_)
                        : nullptr;
  _impl_.ownerid_ = from._impl_.ownerid_;

  // @@protoc_insertion_point(copy_constructor:tgbot.proto.Database)
}
inline PROTOBUF_NDEBUG_INLINE Database::Impl_::Impl_(
    ::google::protobuf::internal::InternalVisibility visibility,
    ::google::protobuf::Arena* arena)
      : _cached_size_{0},
        mediatonames_{visibility, arena} {}

inline void Database::SharedCtor(::_pb::Arena* arena) {
  new (&_impl_) Impl_(internal_visibility(), arena);
  ::memset(reinterpret_cast<char *>(&_impl_) +
               offsetof(Impl_, whitelist_),
           0,
           offsetof(Impl_, ownerid_) -
               offsetof(Impl_, whitelist_) +
               sizeof(Impl_::ownerid_));
}
Database::~Database() {
  // @@protoc_insertion_point(destructor:tgbot.proto.Database)
  _internal_metadata_.Delete<::google::protobuf::UnknownFieldSet>();
  SharedDtor();
}
inline void Database::SharedDtor() {
  ABSL_DCHECK(GetArena() == nullptr);
  delete _impl_.whitelist_;
  delete _impl_.blacklist_;
  _impl_.~Impl_();
}

const ::google::protobuf::MessageLite::ClassData*
Database::GetClassData() const {
  PROTOBUF_CONSTINIT static const ::google::protobuf::MessageLite::
      ClassDataFull _data_ = {
          {
              nullptr,  // OnDemandRegisterArenaDtor
              PROTOBUF_FIELD_OFFSET(Database, _impl_._cached_size_),
              false,
          },
          &Database::MergeImpl,
          &Database::kDescriptorMethods,
      };
  return &_data_;
}
PROTOBUF_NOINLINE void Database::Clear() {
// @@protoc_insertion_point(message_clear_start:tgbot.proto.Database)
  PROTOBUF_TSAN_WRITE(&_impl_._tsan_detect_race);
  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  _impl_.mediatonames_.Clear();
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000003u) {
    if (cached_has_bits & 0x00000001u) {
      ABSL_DCHECK(_impl_.whitelist_ != nullptr);
      _impl_.whitelist_->Clear();
    }
    if (cached_has_bits & 0x00000002u) {
      ABSL_DCHECK(_impl_.blacklist_ != nullptr);
      _impl_.blacklist_->Clear();
    }
  }
  _impl_.ownerid_ = ::int64_t{0};
  _impl_._has_bits_.Clear();
  _internal_metadata_.Clear<::google::protobuf::UnknownFieldSet>();
}

const char* Database::_InternalParse(
    const char* ptr, ::_pbi::ParseContext* ctx) {
  ptr = ::_pbi::TcParser::ParseLoop(this, ptr, ctx, &_table_.header);
  return ptr;
}


PROTOBUF_CONSTINIT PROTOBUF_ATTRIBUTE_INIT_PRIORITY1
const ::_pbi::TcParseTable<2, 4, 3, 0, 2> Database::_table_ = {
  {
    PROTOBUF_FIELD_OFFSET(Database, _impl_._has_bits_),
    0, // no _extensions_
    4, 24,  // max_field_number, fast_idx_mask
    offsetof(decltype(_table_), field_lookup_table),
    4294967280,  // skipmap
    offsetof(decltype(_table_), field_entries),
    4,  // num_field_entries
    3,  // num_aux_entries
    offsetof(decltype(_table_), aux_entries),
    &_Database_default_instance_._instance,
    ::_pbi::TcParser::GenericFallback,  // fallback
    #ifdef PROTOBUF_PREFETCH_PARSE_TABLE
    ::_pbi::TcParser::GetTable<::tgbot::proto::Database>(),  // to_prefetch
    #endif  // PROTOBUF_PREFETCH_PARSE_TABLE
  }, {{
    // repeated .tgbot.proto.MediaToName mediaToNames = 4;
    {::_pbi::TcParser::FastMtR1,
     {34, 63, 2, PROTOBUF_FIELD_OFFSET(Database, _impl_.mediatonames_)}},
    // optional int64 ownerId = 1;
    {::_pbi::TcParser::SingularVarintNoZag1<::uint64_t, offsetof(Database, _impl_.ownerid_), 2>(),
     {8, 2, 0, PROTOBUF_FIELD_OFFSET(Database, _impl_.ownerid_)}},
    // optional .tgbot.proto.PersonList whitelist = 2;
    {::_pbi::TcParser::FastMtS1,
     {18, 0, 0, PROTOBUF_FIELD_OFFSET(Database, _impl_.whitelist_)}},
    // optional .tgbot.proto.PersonList blacklist = 3;
    {::_pbi::TcParser::FastMtS1,
     {26, 1, 1, PROTOBUF_FIELD_OFFSET(Database, _impl_.blacklist_)}},
  }}, {{
    65535, 65535
  }}, {{
    // optional int64 ownerId = 1;
    {PROTOBUF_FIELD_OFFSET(Database, _impl_.ownerid_), _Internal::kHasBitsOffset + 2, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kInt64)},
    // optional .tgbot.proto.PersonList whitelist = 2;
    {PROTOBUF_FIELD_OFFSET(Database, _impl_.whitelist_), _Internal::kHasBitsOffset + 0, 0,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
    // optional .tgbot.proto.PersonList blacklist = 3;
    {PROTOBUF_FIELD_OFFSET(Database, _impl_.blacklist_), _Internal::kHasBitsOffset + 1, 1,
    (0 | ::_fl::kFcOptional | ::_fl::kMessage | ::_fl::kTvTable)},
    // repeated .tgbot.proto.MediaToName mediaToNames = 4;
    {PROTOBUF_FIELD_OFFSET(Database, _impl_.mediatonames_), -1, 2,
    (0 | ::_fl::kFcRepeated | ::_fl::kMessage | ::_fl::kTvTable)},
  }}, {{
    {::_pbi::TcParser::GetTable<::tgbot::proto::PersonList>()},
    {::_pbi::TcParser::GetTable<::tgbot::proto::PersonList>()},
    {::_pbi::TcParser::GetTable<::tgbot::proto::MediaToName>()},
  }}, {{
  }},
};

::uint8_t* Database::_InternalSerialize(
    ::uint8_t* target,
    ::google::protobuf::io::EpsCopyOutputStream* stream) const {
  // @@protoc_insertion_point(serialize_to_array_start:tgbot.proto.Database)
  ::uint32_t cached_has_bits = 0;
  (void)cached_has_bits;

  cached_has_bits = _impl_._has_bits_[0];
  // optional int64 ownerId = 1;
  if (cached_has_bits & 0x00000004u) {
    target = ::google::protobuf::internal::WireFormatLite::
        WriteInt64ToArrayWithField<1>(
            stream, this->_internal_ownerid(), target);
  }

  // optional .tgbot.proto.PersonList whitelist = 2;
  if (cached_has_bits & 0x00000001u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        2, *_impl_.whitelist_, _impl_.whitelist_->GetCachedSize(), target, stream);
  }

  // optional .tgbot.proto.PersonList blacklist = 3;
  if (cached_has_bits & 0x00000002u) {
    target = ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
        3, *_impl_.blacklist_, _impl_.blacklist_->GetCachedSize(), target, stream);
  }

  // repeated .tgbot.proto.MediaToName mediaToNames = 4;
  for (unsigned i = 0, n = static_cast<unsigned>(
                           this->_internal_mediatonames_size());
       i < n; i++) {
    const auto& repfield = this->_internal_mediatonames().Get(i);
    target =
        ::google::protobuf::internal::WireFormatLite::InternalWriteMessage(
            4, repfield, repfield.GetCachedSize(),
            target, stream);
  }

  if (PROTOBUF_PREDICT_FALSE(_internal_metadata_.have_unknown_fields())) {
    target =
        ::_pbi::WireFormat::InternalSerializeUnknownFieldsToArray(
            _internal_metadata_.unknown_fields<::google::protobuf::UnknownFieldSet>(::google::protobuf::UnknownFieldSet::default_instance), target, stream);
  }
  // @@protoc_insertion_point(serialize_to_array_end:tgbot.proto.Database)
  return target;
}

::size_t Database::ByteSizeLong() const {
// @@protoc_insertion_point(message_byte_size_start:tgbot.proto.Database)
  ::size_t total_size = 0;

  ::uint32_t cached_has_bits = 0;
  // Prevent compiler warnings about cached_has_bits being unused
  (void) cached_has_bits;

  // repeated .tgbot.proto.MediaToName mediaToNames = 4;
  total_size += 1UL * this->_internal_mediatonames_size();
  for (const auto& msg : this->_internal_mediatonames()) {
    total_size += ::google::protobuf::internal::WireFormatLite::MessageSize(msg);
  }
  cached_has_bits = _impl_._has_bits_[0];
  if (cached_has_bits & 0x00000007u) {
    // optional .tgbot.proto.PersonList whitelist = 2;
    if (cached_has_bits & 0x00000001u) {
      total_size +=
          1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.whitelist_);
    }

    // optional .tgbot.proto.PersonList blacklist = 3;
    if (cached_has_bits & 0x00000002u) {
      total_size +=
          1 + ::google::protobuf::internal::WireFormatLite::MessageSize(*_impl_.blacklist_);
    }

    // optional int64 ownerId = 1;
    if (cached_has_bits & 0x00000004u) {
      total_size += ::_pbi::WireFormatLite::Int64SizePlusOne(
          this->_internal_ownerid());
    }

  }
  return MaybeComputeUnknownFieldsSize(total_size, &_impl_._cached_size_);
}


void Database::MergeImpl(::google::protobuf::MessageLite& to_msg, const ::google::protobuf::MessageLite& from_msg) {
  auto* const _this = static_cast<Database*>(&to_msg);
  auto& from = static_cast<const Database&>(from_msg);
  ::google::protobuf::Arena* arena = _this->GetArena();
  // @@protoc_insertion_point(class_specific_merge_from_start:tgbot.proto.Database)
  ABSL_DCHECK_NE(&from, _this);
  ::uint32_t cached_has_bits = 0;
  (void) cached_has_bits;

  _this->_internal_mutable_mediatonames()->MergeFrom(
      from._internal_mediatonames());
  cached_has_bits = from._impl_._has_bits_[0];
  if (cached_has_bits & 0x00000007u) {
    if (cached_has_bits & 0x00000001u) {
      ABSL_DCHECK(from._impl_.whitelist_ != nullptr);
      if (_this->_impl_.whitelist_ == nullptr) {
        _this->_impl_.whitelist_ =
            ::google::protobuf::Message::CopyConstruct<::tgbot::proto::PersonList>(arena, *from._impl_.whitelist_);
      } else {
        _this->_impl_.whitelist_->MergeFrom(*from._impl_.whitelist_);
      }
    }
    if (cached_has_bits & 0x00000002u) {
      ABSL_DCHECK(from._impl_.blacklist_ != nullptr);
      if (_this->_impl_.blacklist_ == nullptr) {
        _this->_impl_.blacklist_ =
            ::google::protobuf::Message::CopyConstruct<::tgbot::proto::PersonList>(arena, *from._impl_.blacklist_);
      } else {
        _this->_impl_.blacklist_->MergeFrom(*from._impl_.blacklist_);
      }
    }
    if (cached_has_bits & 0x00000004u) {
      _this->_impl_.ownerid_ = from._impl_.ownerid_;
    }
  }
  _this->_impl_._has_bits_[0] |= cached_has_bits;
  _this->_internal_metadata_.MergeFrom<::google::protobuf::UnknownFieldSet>(from._internal_metadata_);
}

void Database::CopyFrom(const Database& from) {
// @@protoc_insertion_point(class_specific_copy_from_start:tgbot.proto.Database)
  if (&from == this) return;
  Clear();
  MergeFrom(from);
}

PROTOBUF_NOINLINE bool Database::IsInitialized() const {
  return true;
}

void Database::InternalSwap(Database* PROTOBUF_RESTRICT other) {
  using std::swap;
  _internal_metadata_.InternalSwap(&other->_internal_metadata_);
  swap(_impl_._has_bits_[0], other->_impl_._has_bits_[0]);
  _impl_.mediatonames_.InternalSwap(&other->_impl_.mediatonames_);
  ::google::protobuf::internal::memswap<
      PROTOBUF_FIELD_OFFSET(Database, _impl_.ownerid_)
      + sizeof(Database::_impl_.ownerid_)
      - PROTOBUF_FIELD_OFFSET(Database, _impl_.whitelist_)>(
          reinterpret_cast<char*>(&_impl_.whitelist_),
          reinterpret_cast<char*>(&other->_impl_.whitelist_));
}

::google::protobuf::Metadata Database::GetMetadata() const {
  return ::_pbi::AssignDescriptors(&descriptor_table_proto_2fTgBotDB_2eproto_getter,
                                   &descriptor_table_proto_2fTgBotDB_2eproto_once,
                                   file_level_metadata_proto_2fTgBotDB_2eproto[2]);
}
// @@protoc_insertion_point(namespace_scope)
}  // namespace proto
}  // namespace tgbot
namespace google {
namespace protobuf {
}  // namespace protobuf
}  // namespace google
// @@protoc_insertion_point(global_scope)
PROTOBUF_ATTRIBUTE_INIT_PRIORITY2
static ::std::false_type _static_init_ PROTOBUF_UNUSED =
    (::_pbi::AddDescriptors(&descriptor_table_proto_2fTgBotDB_2eproto),
     ::std::false_type{});
#include "google/protobuf/port_undef.inc"
