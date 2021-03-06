// Copyright (c) 2017, Xiaomi, Inc.  All rights reserved.
// This source code is licensed under the Apache License Version 2.0, which
// can be found in the LICENSE file in the root directory of this source tree.

#pragma once

#include <stdint.h>
#include <string.h>
#include <string>
#include <vector>

#include <dsn/utility/ports.h>
#include <dsn/utility/utils.h>
#include <dsn/utility/smart_pointers.h>
#include <dsn/utility/endians.h>
#include <dsn/service_api_c.h>
#include <rocksdb/slice.h>

namespace pegasus {

#define PEGASUS_VALUE_SCHEMA_MAX_VERSION 0

/// Extracts expire_ts from rocksdb value with given version.
/// The value schema must be in v0.
/// \return expire_ts in host endian
inline uint32_t pegasus_extract_expire_ts(int version, dsn::string_view value)
{
    dassert(version <= PEGASUS_VALUE_SCHEMA_MAX_VERSION,
            "value schema version(%d) must be <= %d",
            version,
            PEGASUS_VALUE_SCHEMA_MAX_VERSION);

    return dsn::data_input(value).read_u32();
}

/// Extracts user value from a raw rocksdb value.
/// In order to avoid data copy, the ownership of `raw_value` will be transferred
/// into `user_data`.
/// \param user_data: the result.
inline void pegasus_extract_user_data(int version, std::string &&raw_value, ::dsn::blob &user_data)
{
    dassert(version <= PEGASUS_VALUE_SCHEMA_MAX_VERSION,
            "value schema version(%d) must be <= %d",
            version,
            PEGASUS_VALUE_SCHEMA_MAX_VERSION);

    dsn::data_input input(raw_value);
    input.skip(sizeof(uint32_t));

    dsn::string_view view = input.read_str();

    // tricky code to avoid memory copy
    auto ptr = const_cast<char *>(view.data());
    auto deleter = [s = new std::string(std::move(raw_value))](char *) { delete s; };
    std::shared_ptr<char> buf(ptr, deleter);
    user_data.assign(std::move(buf), 0, static_cast<unsigned int>(view.length()));
}

/// \return true if expired
inline bool check_if_record_expired(uint32_t epoch_now, uint32_t expire_ts)
{
    return expire_ts > 0 && expire_ts <= epoch_now;
}

/// \return true if expired
inline bool check_if_record_expired(uint32_t value_schema_version,
                                    uint32_t epoch_now,
                                    dsn::string_view raw_value)
{
    uint32_t expire_ts = pegasus_extract_expire_ts(value_schema_version, raw_value);
    return check_if_record_expired(epoch_now, expire_ts);
}

/// Helper class for generating value.
/// NOTE that the instance of pegasus_value_generator must be alive
/// while the returned SliceParts is.
class pegasus_value_generator
{
public:
    /// A higher level utility for generating value with given version.
    /// The value schema must be in v0.
    rocksdb::SliceParts
    generate_value(int value_schema_version, dsn::string_view user_data, uint32_t expire_ts)
    {
        if (value_schema_version == 0) {
            return generate_value_v0(expire_ts, user_data);
        } else {
            dfatal("unsupported value schema version: %d", value_schema_version);
            __builtin_unreachable();
        }
    }

    /// The heading expire_ts is encoded to support TTL, and the record will be
    /// automatically cleared (by \see pegasus::server::KeyWithTTLCompactionFilter)
    /// after expiration reached. The expired record will be invisible even though
    /// they are not yet compacted.
    ///
    /// rocksdb value (ver 0) = [expire_ts(uint32_t)] [user_data(bytes)]
    /// \internal
    rocksdb::SliceParts generate_value_v0(uint32_t expire_ts, dsn::string_view user_data)
    {
        _write_buf.resize(sizeof(uint32_t));
        _write_slices.clear();

        dsn::data_output(_write_buf).write_u32(expire_ts);
        _write_slices.emplace_back(_write_buf.data(), _write_buf.size());

        if (user_data.length() > 0) {
            _write_slices.emplace_back(user_data.data(), user_data.length());
        }

        return rocksdb::SliceParts(&_write_slices[0], static_cast<int>(_write_slices.size()));
    }

private:
    std::string _write_buf;
    std::vector<rocksdb::Slice> _write_slices;
};

} // namespace pegasus
