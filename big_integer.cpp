#include "big_integer.h"

big_integer &big_integer::clear() {
    for (size_t i = data_.size() - 1; i > 0 &&
                                      ((data_[i] == 0 && data_[i - 1] < (UINT32_C(1) << 31)) ||
                                       (data_[i] == UINT32_MAX && data_[i - 1] >= (UINT32_C(1) << 31))); i--) {
        data_.pop_back();
    }
    return *this;
}

big_integer &big_integer::invert() {
    data_.push_back(is_positive() ? 0 : UINT32_MAX);
    auto data = data_.data();
    auto size = data_.size();
    for (size_t i = 0; i < size; i++) {
        data[i] = ~data[i];
    }
    for (size_t i = 0; i < size; i++) {
        if (data[i] != UINT32_MAX) {
            data[i]++;
            break;
        }
        data[i] = 0;
    }
    return clear();
}

bool big_integer::is_positive() const {
    return data_[data_.size() - 1] < (UINT32_C(1) << 31);
}

char big_integer::div10() {
    uint64_t tmp_next = 0;
    auto data = data_.data();
    auto size = data_.size();
    for (size_t i = size; i--;) {
        tmp_next = (tmp_next << 32) + data[i];
        data[i] = static_cast<uint32_t>(tmp_next / 10);
        tmp_next %= 10;
    }
    clear();
    return static_cast<char>(tmp_next + '0');
}

big_integer &big_integer::to_right_by32(uint32_t const &rhs) {
    if (!rhs) {
        return *this;
    }
    if (rhs >= data_.size()) {
        data_ = uint_vector(1, is_positive() ? 0 : UINT32_MAX);
    } else {
        data_.begin_erase(rhs);
    }
    return *this;
}

big_integer &big_integer::to_right(uint32_t const &rhs) {
    if (!*this) {
        return *this;
    }
    this->to_right_by32(rhs >> 5);
    size_t shift = rhs % 32;
    if (!shift) {
        return *this;
    }
    auto data = data_.data();
    auto size = data_.size();
    for (size_t i = 0; i < size - 1; i++) {
        data[i] = (data[i] >> shift) + (data[i + 1] << (32 - shift));
    }
    data[size - 1] = (data[size - 1] >> shift) + (is_positive() ? 0 : (UINT32_MAX << (32 - shift)));
    return clear();
}

big_integer &big_integer::to_left_by32(uint32_t const &rhs) {
    if (!rhs) {
        return *this;
    }
    data_.begin_insert(rhs, 0);
    return *this;
}

big_integer &big_integer::to_left(uint32_t const &rhs) {
    if (!*this) {
        return *this;
    }
    this->to_left_by32(rhs >> 5);
    size_t shift = rhs % 32;
    if (!shift) {
        return *this;
    }
    data_.push_back(is_positive() ? 0 : UINT32_MAX);
    uint64_t tmp_next = 0;
    auto data = data_.data();
    auto size = data_.size();
    for (size_t i = 0; i < size; i++) {
        tmp_next += static_cast<uint64_t>(data[i]) << shift;
        data[i] = static_cast<uint32_t>(tmp_next);
        tmp_next >>= 32;
    }
    return clear();
}

big_integer &big_integer::division(big_integer const &rhs) {
    if (!rhs || rhs > *this) {
        return *this = big_integer(0);
    }
    if (rhs == 1) {
        return *this;
    }

    if (*this < rhs) {
        this->data_.assign(1, 0);
        return *this;
    }
    auto data = data_.data();
    auto size = data_.size();
    auto rhs_data = rhs.data_.data();
    auto rhs_size = rhs.data_.size();
    if (rhs_size == 1 || (rhs_size == 2 && rhs_data[1] == 0)) {
        big_integer result;
        uint64_t rhs_tmp = rhs_data[0];
        uint64_t tmp_next = 0;
        for (size_t pos = size; pos--;) {
            tmp_next += data[pos];
            data[pos] = static_cast<uint32_t>(tmp_next / rhs_tmp);
            tmp_next = (tmp_next % rhs_tmp) << 32;
        }
        return clear();
    }

    size_t pos = size - rhs_size;
    big_integer result;
    big_integer midrhs;
    result.data_.assign(pos + 2, 0);
    auto res_data = result.data_.data();
    for (pos += 1; pos--;) {
        uint32_t tmp_divider_left = 0;
        uint32_t tmp_divider_right = UINT32_MAX;
        while (tmp_divider_left + 1 < tmp_divider_right) {
            uint32_t mid = tmp_divider_left + (tmp_divider_right - tmp_divider_left) / 2;
            midrhs = multiply(rhs, mid).to_left_by32(pos);

            if (*this < midrhs) {
                tmp_divider_right = mid - 1;
            } else if (*this == midrhs) {
                res_data[pos] = mid;
                return *this = result.clear();
            } else {
                tmp_divider_left = mid;
            }
        }
        if (tmp_divider_left + 1 == tmp_divider_right) {
            res_data[pos] = tmp_divider_right;
            if (*this < multiply(rhs, tmp_divider_right).to_left_by32(pos)) {
                res_data[pos] = tmp_divider_left;
            }
        } else {
            res_data[pos] = tmp_divider_right;
        }
        operator-=(multiply(rhs, res_data[pos]).to_left_by32(pos));
    }
    return *this = result.clear();
}

big_integer::big_integer(std::string const &rhs) {
    if (rhs.length() == 0 || (rhs.length() == 1 && rhs.front() == '-')) {
        data_.push_back(0);
        return;
    }
    size_t pos = 0;

    if (rhs.front() == '-') {
        pos = 1;
    }

    std::vector<uint32_t> mods(1, 1);
    uint64_t tmp_next_mod;

    for (size_t i = rhs.length(), next_i = pos + 1; next_i != pos; i = next_i) {
        next_i = (i > 9 + pos) ? (i - 9) : pos;
        uint64_t tmp_num = static_cast<uint64_t>(std::stoi(rhs.substr(next_i, i - next_i)));

        tmp_next_mod = 0;
        for (size_t j = 0; j < mods.size() || tmp_next_mod > 0; j++) {
            if (j == data_.size()) {
                data_.push_back(0);
            }
            tmp_next_mod += ((j < mods.size()) ? tmp_num * mods[j] : 0) + data_[j];
            data_[j] = static_cast<uint32_t>(tmp_next_mod);
            tmp_next_mod >>= 32;
        }

        tmp_next_mod = 0;
        for (size_t j = 0; j < mods.size(); j++) {
            tmp_next_mod += UINT64_C(1000000000) * mods[j];
            mods[j] = static_cast<uint32_t>(tmp_next_mod);
            tmp_next_mod >>= 32;
            if (j + 1 == mods.size() && tmp_next_mod > 0) {
                mods.push_back(static_cast<uint32_t>(tmp_next_mod));
                break;
            }
        }
    }

    if (!is_positive()) {
        data_.push_back(0);
    }
    if (pos == 1) {
        invert();
    }
    clear();
}

big_integer::big_integer(int32_t rhs) {
    data_.push_back(static_cast<uint32_t>(rhs));
}

big_integer &big_integer::operator=(big_integer const &rhs) {
    if (this != &rhs) {
        data_ = rhs.data_;
    }
    return *this;
}

big_integer &big_integer::operator+=(big_integer const &rhs) {
    uint32_t lhs_saved = is_positive() ? 0 : UINT32_MAX;
    uint32_t rhs_saved = rhs.is_positive() ? 0 : UINT32_MAX;
    auto size = data_.size();

    auto rhs_size = rhs.data_.size();
    size_t length = 1 + ((size > rhs_size) ? size : rhs_size);
    uint64_t tmp_next = 0;
    for (size_t i = 0; i < length - size; i++) {
        data_.push_back(lhs_saved);
    }
    auto data = data_.data();
    auto rhs_data = rhs.data_.data();
    for (size_t i = 0; i < length; i++) {
        tmp_next += static_cast<uint64_t>(data[i]) + ((i < rhs_size) ? rhs_data[i] : rhs_saved);
        data[i] = static_cast<uint32_t>(tmp_next);
        tmp_next >>= 32;
    }
    return clear();
}

big_integer &big_integer::operator-=(big_integer const &rhs) {
    invert();
    operator+=(rhs);
    return invert();
}

big_integer multiply(big_integer const &lhs, uint64_t const &rhs) {
    if (!rhs) {
        return big_integer();
    }
    big_integer lhs_new(lhs);
    lhs_new.data_.push_back(0);
    uint64_t tmp_next = 0;
    auto lhs_new_data = lhs_new.data_.data();
    auto lhs_new_size = lhs_new.data_.size();
    for (size_t i = 0; i < lhs_new_size; i++) {
        tmp_next += rhs * lhs_new_data[i];
        lhs_new_data[i] = static_cast<uint32_t>(tmp_next);
        tmp_next >>= 32;
    }
    return lhs_new.clear();
}

big_integer &big_integer::operator*=(big_integer const &rhs) {
    if (rhs == 0) {
        return *this = 0;
    }
    bool lhs_positive = is_positive();
    bool rhs_positive = rhs.is_positive();
    if (!lhs_positive && !rhs_positive) {
        return invert() *= big_integer(rhs).invert();
    }
    if (!lhs_positive && rhs_positive) {
        invert() *= rhs;
        return invert();
    }
    if (lhs_positive && !rhs_positive) {
        operator*=(big_integer(rhs).invert());
        return invert();
    }
    auto rhs_data = rhs.data_.data();
    auto rhs_size = rhs.data_.size();

    if (rhs_size < 2 || (rhs_size == 2 && rhs_data[1] == 0)) {
        data_.push_back(0);
        auto data = data_.data();
        auto size = data_.size();
        uint64_t rhs_tmp = static_cast<uint64_t >(rhs_data[0]);
        uint64_t tmp_next = 0;
        for (size_t i = 0; i < size; i++) {
            tmp_next += rhs_tmp * data[i];
            data[i] = static_cast<uint32_t>(tmp_next);
            tmp_next >>= 32;
        }
        return clear();
    }
    big_integer accumulate;
    big_integer tmp = *this;
    for (size_t i = 0; i < rhs_size; i++) {
        if (rhs_data[i]) {
            accumulate += multiply(tmp, rhs_data[i]);
        }
        tmp.to_left_by32(1);
    }
    this->data_ = accumulate.clear().data_;

    return *this;
}

big_integer &big_integer::operator/=(big_integer const &rhs) {
    bool lhs_positive = is_positive();
    bool rhs_positive = rhs.is_positive();
    if (!lhs_positive && !rhs_positive) {
        return invert().division(big_integer(rhs).invert());
    }
    if (!lhs_positive && rhs_positive) {
        return invert().division(rhs).invert();
    }
    if (lhs_positive && !rhs_positive) {
        return division(big_integer(rhs).invert()).invert();
    }
    return division(rhs);
}

big_integer &big_integer::operator%=(big_integer const &rhs) {
    return operator-=((*this / rhs) * rhs);
}

big_integer &big_integer::operator&=(big_integer const &rhs) {

    uint32_t lhs_saved = is_positive() ? 0 : UINT32_MAX;
    uint32_t rhs_saved = rhs.is_positive() ? 0 : UINT32_MAX;
    size_t length = (data_.size() > rhs.data_.size()) ? data_.size() : rhs.data_.size();
    for (size_t i = 0; i < length - data_.size(); i++) {
        data_.push_back(lhs_saved);
    }
    auto data = data_.data();
    auto rhs_data = rhs.data_.data();
    auto rhs_size = rhs.data_.size();
    for (size_t i = 0; i < length; i++) {
        data[i] &= (i < rhs_size) ? rhs_data[i] : rhs_saved;
    }
    return clear();
}

big_integer &big_integer::operator|=(big_integer const &rhs) {

    uint32_t lhs_saved = is_positive() ? 0 : UINT32_MAX;
    uint32_t rhs_saved = rhs.is_positive() ? 0 : UINT32_MAX;
    size_t length = (data_.size() > rhs.data_.size()) ? data_.size() : rhs.data_.size();
    for (size_t i = 0; i < length - data_.size(); i++) {
        data_.push_back(lhs_saved);
    }
    auto data = data_.data();
    auto rhs_data = rhs.data_.data();
    auto rhs_size = rhs.data_.size();
    for (size_t i = 0; i < length; i++) {
        data[i] |= (i < rhs_size) ? rhs_data[i] : rhs_saved;
    }
    return clear();
}

big_integer &big_integer::operator^=(big_integer const &rhs) {

    uint32_t lhs_saved = is_positive() ? 0 : UINT32_MAX;
    uint32_t rhs_saved = rhs.is_positive() ? 0 : UINT32_MAX;
    size_t length = (data_.size() > rhs.data_.size()) ? data_.size() : rhs.data_.size();
    for (size_t i = 0; i < length - data_.size(); i++) {
        data_.push_back(lhs_saved);
    }
    auto data = data_.data();
    auto rhs_data = rhs.data_.data();
    auto rhs_size = rhs.data_.size();
    for (size_t i = 0; i < length; i++) {
        data[i] ^= (i < rhs_size) ? rhs_data[i] : rhs_saved;
    }
    return clear();
}

big_integer &big_integer::operator>>=(int32_t const &rhs) {
    if (!rhs) {
        return *this;
    }
    return rhs > 0 ? to_right(rhs) : to_left(-rhs);
}

big_integer &big_integer::operator<<=(int32_t const &rhs) {
    if (!rhs) {
        return *this;
    }
    return rhs > 0 ? to_left(rhs) : to_right(-rhs);
}

big_integer &big_integer::operator++() {
    return operator+=(1);
}

big_integer &big_integer::operator--() {
    return operator+=(-1);
}

big_integer const big_integer::operator++(int) {
    big_integer tmp(*this);
    operator++();
    return tmp;
}

big_integer const big_integer::operator--(int) {
    big_integer tmp(*this);
    operator--();
    return tmp;
}

big_integer operator+(big_integer const &lhs) {
    return lhs;
}

big_integer operator-(big_integer const &lhs) {
    return big_integer(lhs).invert();
}

big_integer operator~(big_integer const &lhs) {
    big_integer tmp(lhs);
    auto tmp_data = tmp.data_.data();
    auto tmp_size = tmp.data_.size();
    for (size_t i = 0; i < tmp_size; i++) {
        tmp_data[i] = ~tmp_data[i];
    }
    return tmp;
}

big_integer operator+(big_integer const &lhs, big_integer const &rhs) {
    return big_integer(lhs) += rhs;
}

big_integer operator-(big_integer const &lhs, big_integer const &rhs) {
    return big_integer(lhs) -= rhs;
}

big_integer operator*(big_integer const &lhs, big_integer const &rhs) {
    return big_integer(lhs) *= rhs;
}

big_integer operator/(big_integer const &lhs, big_integer const &rhs) {
    return big_integer(lhs) /= rhs;
}

big_integer operator%(big_integer const &lhs, big_integer const &rhs) {
    return big_integer(lhs) %= rhs;
}

big_integer operator&(big_integer const &lhs, big_integer const &rhs) {
    return big_integer(lhs) &= rhs;
}

big_integer operator|(big_integer const &lhs, big_integer const &rhs) {
    return big_integer(lhs) |= rhs;
}

big_integer operator^(big_integer const &lhs, big_integer const &rhs) {
    return big_integer(lhs) ^= rhs;
}

big_integer operator<<(big_integer const &lhs, int32_t const &rhs) {
    return big_integer(lhs) <<= rhs;
}

big_integer operator>>(big_integer const &lhs, int32_t const &rhs) {
    return big_integer(lhs) >>= rhs;
}

bool operator!(big_integer const &lhs) {
    return lhs == 0;
}

bool operator&&(big_integer const &lhs, big_integer const &rhs) {
    return (lhs != 0 && rhs != 0);
}

bool operator||(big_integer const &lhs, big_integer const &rhs) {
    return (lhs != 0 || rhs != 0);
}

bool operator==(big_integer const &lhs, big_integer const &rhs) {
    return lhs.data_ == rhs.data_;
}

bool operator!=(big_integer const &lhs, big_integer const &rhs) {
    return lhs.data_ != rhs.data_;
}

bool operator>(big_integer const &lhs, big_integer const &rhs) {
    bool lhs_positive = lhs.is_positive();
    bool rhs_positive = rhs.is_positive();
    if (lhs_positive && !rhs_positive) {
        return true;
    }
    if (!lhs_positive && rhs_positive) {
        return false;
    }
    if (!lhs_positive && !rhs_positive) {
        return (lhs - rhs).is_positive();
    }
    if (lhs.data_.size() > rhs.data_.size()) {
        return true;
    }
    if (lhs.data_.size() < rhs.data_.size()) {
        return false;
    }
    for (size_t i = rhs.data_.size(); i--;) {
        if (rhs.data_[i] > lhs.data_[i]) {
            return false;
        }
        if (rhs.data_[i] < lhs.data_[i]) {
            return true;
        }
    }
    return false;
}

bool operator<(big_integer const &lhs, big_integer const &rhs) {
    return rhs > lhs;
}

bool operator>=(big_integer const &lhs, big_integer const &rhs) {
    return !(rhs > lhs);
}

bool operator<=(big_integer const &lhs, big_integer const &rhs) {
    return !(lhs > rhs);
}

std::string to_string(big_integer rhs) {
    std::string tmp;
    bool minus = false;
    if (!rhs.is_positive()) {
        minus = true;
        rhs.invert();
    }
    if (rhs == 0) {
        tmp = "0";
        return tmp;
    }
    while (rhs != 0) {
        tmp.push_back(rhs.div10());
    }
    if (minus) {
        tmp.push_back('-');
    }
    std::reverse(tmp.begin(), tmp.end());
    return tmp;
}

std::ostream &operator<<(std::ostream &cout, big_integer rhs) {

    cout << to_string(rhs);
    return cout;
}

std::istream &operator>>(std::istream &cin, big_integer &rhs) {
    std::string tmp;
    cin >> tmp;
    rhs = big_integer(tmp);
    return cin;
}
