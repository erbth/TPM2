/** This file is part of the TSClient LEGACY Package Manager
 *
 * This module deals with dependency-package version constraining predicate
 * logics. */

#ifndef __PACKAGE_CONSTRAINTS_H
#define __PACKAGE_CONSTRAINTS_H

#include <exception>
#include <memory>
#include <string>
#include "version_number.h"


namespace PackageConstraints
{
	/* An abstract base class to represent primitive predicates and more complex
	 * formulas */
	class Formula
	{
	public:
		virtual ~Formula();

		/* Tests if the formula is fulfilled by the given source- and binary version
		 * numbers.
		 *
		 * @sv Source version number
		 * @bv Binary version number */
		virtual bool fulfilled(const VersionNumber &sv, const VersionNumber &bv) const = 0;

		/* Returns an invertible string representation. true and false maybe
		 * represented by And(nullptr, nullptr) and Or(nullptr, nullptr),
		 * respectively. */
		virtual std::string to_string() const = 0;

		/* Converts a string to a formula. Returns nullptr if the string is
		 * invalid. */
		static std::shared_ptr<Formula> from_string(const std::string&);
	};


	class PrimitivePredicate : public Formula
	{
	public:
		static const char TYPE_EQ = 0;
		static const char TYPE_NEQ = 1;
		static const char TYPE_GEQ = 2;
		static const char TYPE_LEQ = 3;
		static const char TYPE_GT = 4;
		static const char TYPE_LT = 5;

	private:
		const bool is_source;
		const char type;

		const VersionNumber v;

	public:
		PrimitivePredicate(bool is_source, char type, const VersionNumber &v);
		virtual ~PrimitivePredicate();

		bool fulfilled(const VersionNumber &sv, const VersionNumber &bv) const override;

		std::string to_string() const override;
	};


	class And : public Formula
	{
	private:
		const std::shared_ptr<const Formula> left;
		const std::shared_ptr<const Formula> right;

	public:
		/* nullptr means neutral element, however on its own it's not a valid
		 * formula */
		And(std::shared_ptr<const Formula> left, std::shared_ptr<const Formula> right);
		virtual ~And();

		bool fulfilled(const VersionNumber &sv, const VersionNumber &bv) const override;

		std::string to_string() const override;
	};


	class Or : public Formula
	{
	private:
		const std::shared_ptr<const Formula> left;
		const std::shared_ptr<const Formula> right;

	public:
		/* nullptr means neutral element, however on its own it's not a valid
		 * formula */
		Or(std::shared_ptr<const Formula> left, std::shared_ptr<const Formula> right);
		virtual ~Or();

		bool fulfilled(const VersionNumber &sv, const VersionNumber &bv) const override;

		std::string to_string() const override;
	};
}

#endif /* __PACKAGE_CONSTRAINTS_H */
