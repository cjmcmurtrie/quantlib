/* -*- mode: c++; tab-width: 4; indent-tabs-mode: nil; c-basic-offset: 4 -*- */

/*
 Copyright (C) 2006 Giorgio Facchinetti
 Copyright (C) 2006 Mario Pucci
 Copyright (C) 2000, 2001, 2002, 2003 RiskMap srl
 Copyright (C) 2003, 2004, 2005 StatPro Italia srl

 This file is part of QuantLib, a free-software/open-source library
 for financial quantitative analysts and developers - http://quantlib.org/

 QuantLib is free software: you can redistribute it and/or modify it
 under the terms of the QuantLib license.  You should have received a
 copy of the license along with this program; if not, please email
 <quantlib-dev@lists.sf.net>. The license is also available online at
 <http://quantlib.org/reference/license.html>.

 This program is distributed in the hope that it will be useful, but WITHOUT
 ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
 FOR A PARTICULAR PURPOSE.  See the license for more details.
*/

#include <ql/CashFlows/cashflowvectors.hpp>
#include <ql/CashFlows/fixedratecoupon.hpp>
#include <ql/CashFlows/shortfloatingcoupon.hpp>
#include <ql/CashFlows/upfrontindexedcoupon.hpp>
#include <ql/CashFlows/indexedcashflowvectors.hpp>

namespace QuantLib {

    std::vector<boost::shared_ptr<CashFlow> >
    FixedRateCouponVector(const Schedule& schedule,
                          BusinessDayConvention paymentAdjustment,
                          const std::vector<Real>& nominals,
                          const std::vector<Rate>& couponRates,
                          const DayCounter& dayCount,
                          const DayCounter& firstPeriodDayCount) {

        QL_REQUIRE(!couponRates.empty(), "coupon rates not specified");
        QL_REQUIRE(!nominals.empty(), "nominals not specified");

        std::vector<boost::shared_ptr<CashFlow> > leg;
        Calendar calendar = schedule.calendar();

        // first period might be short or long
        Date start = schedule.date(0), end = schedule.date(1);
        Date paymentDate = calendar.adjust(end,paymentAdjustment);
        Rate rate = couponRates[0];
        Real nominal = nominals[0];
        if (schedule.isRegular(1)) {
            QL_REQUIRE(firstPeriodDayCount.empty() ||
                       firstPeriodDayCount == dayCount,
                       "regular first coupon "
                       "does not allow a first-period day count");
            leg.push_back(boost::shared_ptr<CashFlow>(
                new FixedRateCoupon(nominal, paymentDate, rate, dayCount,
                                    start, end, start, end)));
        } else {
            Date reference = end - schedule.tenor();
            reference = calendar.adjust(reference,
                                        schedule.businessDayConvention());
            DayCounter dc = firstPeriodDayCount.empty() ?
                            dayCount :
                            firstPeriodDayCount;
            leg.push_back(boost::shared_ptr<CashFlow>(
                new FixedRateCoupon(nominal, paymentDate, rate,
                                    dc, start, end, reference, end)));
        }
        // regular periods
        for (Size i=2; i<schedule.size()-1; i++) {
            start = end; end = schedule.date(i);
            paymentDate = calendar.adjust(end,paymentAdjustment);
            if ((i-1) < couponRates.size())
                rate = couponRates[i-1];
            else
                rate = couponRates.back();
            if ((i-1) < nominals.size())
                nominal = nominals[i-1];
            else
                nominal = nominals.back();
            leg.push_back(boost::shared_ptr<CashFlow>(
                new FixedRateCoupon(nominal, paymentDate, rate, dayCount,
                                    start, end, start, end)));
        }
        if (schedule.size() > 2) {
            // last period might be short or long
            Size N = schedule.size();
            start = end; end = schedule.date(N-1);
            paymentDate = calendar.adjust(end,paymentAdjustment);
            if ((N-2) < couponRates.size())
                rate = couponRates[N-2];
            else
                rate = couponRates.back();
            if ((N-2) < nominals.size())
                nominal = nominals[N-2];
            else
                nominal = nominals.back();
            if (schedule.isRegular(N-1)) {
                leg.push_back(boost::shared_ptr<CashFlow>(
                    new FixedRateCoupon(nominal, paymentDate,
                                        rate, dayCount,
                                        start, end, start, end)));
            } else {
                Date reference = start + schedule.tenor();
                reference = calendar.adjust(reference,
                                            schedule.businessDayConvention());
                leg.push_back(boost::shared_ptr<CashFlow>(
                    new FixedRateCoupon(nominal, paymentDate,
                                        rate, dayCount,
                                        start, end, start, reference)));
            }
        }
        return leg;
    }

    std::vector<boost::shared_ptr<CashFlow> >
    FloatingRateCouponVector(const Schedule& schedule,
                             const BusinessDayConvention paymentAdjustment,
                             const std::vector<Real>& nominals,
                             const Integer fixingDays,
                             const boost::shared_ptr<Xibor>& index,
                             const std::vector<Real>& gearings,
                             const std::vector<Spread>& spreads,
                             const DayCounter& dayCounter) {

        #ifdef QL_USE_INDEXED_COUPON
        typedef UpFrontIndexedCoupon coupon_type;
        #else
        typedef ParCoupon coupon_type;
        #endif

        std::vector<boost::shared_ptr<CashFlow> > leg =
            IndexedCouponVector<coupon_type>(schedule,
                                             paymentAdjustment,
                                             nominals,
                                             fixingDays,
                                             index,
                                             gearings,
                                             spreads,
                                             dayCounter
                                             #ifdef QL_PATCH_MSVC6
                                             , (const coupon_type*) 0
                                             #endif
                                             );
        return leg;
    }


    namespace {

        Real get(const std::vector<Real>& v, Size i,
                 Real defaultValue = Null<Real>()) {
            if (v.empty()) {
                return defaultValue;
            } else if (i < v.size()) {
                return v[i];
            } else {
                return v.back();
            }
        }

    }

    std::vector<boost::shared_ptr<CashFlow> >
    CMSCouponVector(const Schedule& schedule,
                    BusinessDayConvention paymentAdjustment,
                    const std::vector<Real>& nominals,
                    const boost::shared_ptr<SwapIndex>& index,
                    Integer settlementDays,
                    const DayCounter& dayCounter,
                    const std::vector<Real>& gearings,
                    const std::vector<Spread>& spreads,
                    const std::vector<Rate>& caps,
                    const std::vector<Rate>& floors,
                    const std::vector<Real>& meanReversions,
                    const boost::shared_ptr<VanillaCMSCouponPricer>& pricer,
                    const Handle<SwaptionVolatilityStructure>& vol) {

        //std::vector<CMSCoupon> leg;
        std::vector<boost::shared_ptr<CashFlow> > leg;
        //std::vector<boost::shared_ptr<CashFlow> > legCashFlow;
        Calendar calendar = schedule.calendar();
        Size N = schedule.size();

        QL_REQUIRE(!nominals.empty(), "no nominal given");

        // first period might be short or long
        Date start = schedule.date(0), end = schedule.date(1);
        Date paymentDate = calendar.adjust(end,paymentAdjustment);
        if (schedule.isRegular(1)) {
            leg.push_back(boost::shared_ptr<CashFlow>(
                new CMSCoupon(get(nominals,0), paymentDate, index,
                              start, end, settlementDays, dayCounter, pricer,
                              get(gearings,0,1.0),
                              get(spreads,0,0.0),
                              get(caps,0,Null<Rate>()),
                              get(floors,0,Null<Rate>()),
                              get(meanReversions,0,Null<Rate>()),
                              start, end)));

        } else {
            Date reference = end - schedule.tenor();
            reference =
                calendar.adjust(reference,paymentAdjustment);
            leg.push_back(boost::shared_ptr<CashFlow>(
                new CMSCoupon(get(nominals,0), paymentDate, index,
                              start, end, settlementDays, dayCounter, pricer,
                              get(gearings,0,1.0),
                              get(spreads,0,0.0),
                              get(caps,0,Null<Rate>()),
                              get(floors,0,Null<Rate>()),
                              get(meanReversions,0,Null<Rate>()),
                              reference, end)));
        }
        // regular periods
        for (Size i=2; i<schedule.size()-1; i++) {
            start = end; end = schedule.date(i);
            paymentDate = calendar.adjust(end,paymentAdjustment);
            leg.push_back(boost::shared_ptr<CashFlow>(
                new CMSCoupon(get(nominals,i-1), paymentDate, index,
                              start, end, settlementDays, dayCounter, pricer,
                              get(gearings,i-1,1.0),
                              get(spreads,i-1,0.0),
                              get(caps,i-1,Null<Rate>()),
                              get(floors,i-1,Null<Rate>()),
                              get(meanReversions,i-1,Null<Rate>()),
                              start, end)));
        }
        if (schedule.size() > 2) {
            // last period might be short or long
            start = end; end = schedule.date(N-1);
            paymentDate = calendar.adjust(end,paymentAdjustment);
            if (schedule.isRegular(N-1)) {
                leg.push_back(boost::shared_ptr<CashFlow>(
                    new CMSCoupon(get(nominals,N-2), paymentDate, index,
                                  start, end, settlementDays, dayCounter, pricer,
                                  get(gearings,N-2,1.0),
                                  get(spreads,N-2,0.0),
                                  get(caps,N-2,Null<Rate>()),
                                  get(floors,N-2,Null<Rate>()),
                                  get(meanReversions,N-2,Null<Rate>()),
                                  start, end)));
            } else {
                Date reference = start + schedule.tenor();
                reference =
                    calendar.adjust(reference,paymentAdjustment);
                leg.push_back(boost::shared_ptr<CashFlow>(
                    new CMSCoupon(get(nominals,N-2), paymentDate, index,
                                  start, end, settlementDays, dayCounter, pricer,
                                  get(gearings,N-2,1.0),
                                  get(spreads,N-2,0.0),
                                  get(caps,N-2,Null<Rate>()),
                                  get(floors,N-2,Null<Rate>()),
                                  get(meanReversions,N-2,Null<Rate>()),
                                  start, reference)));
            }
        }

        for (Size i=0; i<leg.size(); i++) {
            const boost::shared_ptr<CMSCoupon> cmsCoupon =
               boost::dynamic_pointer_cast<CMSCoupon>(leg[i]);
            if (cmsCoupon)
                cmsCoupon->setSwaptionVolatility(vol);
            else
                QL_FAIL("unexpected error when casting to CMSCoupon");
        }

        return leg;
    }

    std::vector<boost::shared_ptr<CashFlow> >
    CMSZeroCouponVector(const Schedule& schedule,
                    BusinessDayConvention paymentAdjustment,
                    const std::vector<Real>& nominals,
                    const boost::shared_ptr<SwapIndex>& index,
                    Integer fixingDays,
                    const DayCounter& dayCounter,
                    const std::vector<Real>& gearings,
                    const std::vector<Spread>& spreads,
                    const std::vector<Rate>& caps,
                    const std::vector<Rate>& floors,
                    const std::vector<Real>& meanReversions,
                    const boost::shared_ptr<VanillaCMSCouponPricer>& pricer,
                    const Handle<SwaptionVolatilityStructure>& vol) {

        std::vector<boost::shared_ptr<CashFlow> > leg;
        Calendar calendar = schedule.calendar();
        Size N = schedule.size();

        QL_REQUIRE(!nominals.empty(), "no nominal given");

        // All payment dates at the end
        Date paymentDate = calendar.adjust(schedule.date(N-1),paymentAdjustment);

        // first period might be short or long
        Date start = schedule.date(0), end = schedule.date(1);
        if (schedule.isRegular(1)) {
            leg.push_back(boost::shared_ptr<CashFlow>(
                new CMSCoupon(get(nominals,0), paymentDate, index,
                              start, end, fixingDays, dayCounter, pricer,
                              get(gearings,0,1.0),
                              get(spreads,0,0.0),
                              get(caps,0,Null<Rate>()),
                              get(floors,0,Null<Rate>()),
                              get(meanReversions,0,Null<Rate>()),
                              start, end)));

        } else {
            Date reference = end - schedule.tenor();
            reference =
                calendar.adjust(reference,paymentAdjustment);
            leg.push_back(boost::shared_ptr<CashFlow>(
                new CMSCoupon(get(nominals,0), paymentDate, index,
                              start, end, fixingDays, dayCounter, pricer,
                              get(gearings,0,1.0),
                              get(spreads,0,0.0),
                              get(caps,0,Null<Rate>()),
                              get(floors,0,Null<Rate>()),
                              get(meanReversions,0,Null<Rate>()),
                              reference, end)));
        }
        // regular periods
        for (Size i=2; i<schedule.size()-1; i++) {
            start = end; end = schedule.date(i);
            leg.push_back(boost::shared_ptr<CashFlow>(
                new CMSCoupon(get(nominals,i-1), paymentDate, index,
                              start, end, fixingDays, dayCounter, pricer,
                              get(gearings,i-1,1.0),
                              get(spreads,i-1,0.0),
                              get(caps,i-1,Null<Rate>()),
                              get(floors,i-1,Null<Rate>()),
                              get(meanReversions,i-1,Null<Rate>()),
                              start, end)));
        }
        if (schedule.size() > 2) {
            // last period might be short or long
            start = end; end = schedule.date(N-1);
            if (schedule.isRegular(N-1)) {
                leg.push_back(boost::shared_ptr<CashFlow>(
                    new CMSCoupon(get(nominals,N-2), paymentDate, index,
                                  start, end, fixingDays, dayCounter, pricer,
                                  get(gearings,N-2,1.0),
                                  get(spreads,N-2,0.0),
                                  get(caps,N-2,Null<Rate>()),
                                  get(floors,N-2,Null<Rate>()),
                                  get(meanReversions,N-2,Null<Rate>()),
                                  start, end)));
            } else {
                Date reference = start + schedule.tenor();
                reference =
                    calendar.adjust(reference,paymentAdjustment);
                leg.push_back(boost::shared_ptr<CashFlow>(
                    new CMSCoupon(get(nominals,N-2), paymentDate, index,
                                  start, end, fixingDays, dayCounter, pricer,
                                  get(gearings,N-2,1.0),
                                  get(spreads,N-2,0.0),
                                  get(caps,N-2,Null<Rate>()),
                                  get(floors,N-2,Null<Rate>()),
                                  get(meanReversions,N-2,Null<Rate>()),
                                  start, reference)));
            }
        }

        for (Size i=0; i<leg.size(); i++) {
            const boost::shared_ptr<CMSCoupon> cmsCoupon =
               boost::dynamic_pointer_cast<CMSCoupon>(leg[i]);
            if (cmsCoupon)
                cmsCoupon->setSwaptionVolatility(vol);
            else
                QL_FAIL("unexpected error when casting to CMSCoupon");
        }

        return leg;
    }



    std::vector<boost::shared_ptr<CashFlow> >
    CMSInArrearsCouponVector(const Schedule& schedule,
                    BusinessDayConvention paymentAdjustment,
                    const std::vector<Real>& nominals,
                    const boost::shared_ptr<SwapIndex>& index,
                    Integer fixingDays,
                    const DayCounter& dayCounter,
                    const std::vector<Real>& gearings,
                    const std::vector<Spread>& spreads,
                    const std::vector<Rate>& caps,
                    const std::vector<Rate>& floors,
                    const std::vector<Real>& meanReversions,
                    const boost::shared_ptr<VanillaCMSCouponPricer>& pricer,
                    const Handle<SwaptionVolatilityStructure>& vol) {

        //std::vector<CMSCoupon> leg;
        std::vector<boost::shared_ptr<CashFlow> > leg;
        //std::vector<boost::shared_ptr<CashFlow> > legCashFlow;
        Calendar calendar = schedule.calendar();
        Size N = schedule.size();

        QL_REQUIRE(!nominals.empty(), "no nominal given");

        // first period might be short or long
        Date start = schedule.date(0), end = schedule.date(1);
        Date paymentDate = calendar.adjust(end,paymentAdjustment);
        if (schedule.isRegular(1)) {
            leg.push_back(boost::shared_ptr<CashFlow>(
                new CMSCoupon(get(nominals,0), paymentDate, index,
                              start, end, fixingDays, dayCounter, pricer,
                              get(gearings,0,1.0),
                              get(spreads,0,0.0),
                              get(caps,0,Null<Rate>()),
                              get(floors,0,Null<Rate>()),
                              get(meanReversions,0,Null<Rate>()),
                              start, end, true)));

        } else {
            Date reference = end - schedule.tenor();
            reference =
                calendar.adjust(reference,paymentAdjustment);
            leg.push_back(boost::shared_ptr<CashFlow>(
                new CMSCoupon(get(nominals,0), paymentDate, index,
                              start, end, fixingDays, dayCounter, pricer,
                              get(gearings,0,1.0),
                              get(spreads,0,0.0),
                              get(caps,0,Null<Rate>()),
                              get(floors,0,Null<Rate>()),
                              get(meanReversions,0,Null<Rate>()),
                              reference, end, true)));
        }
        // regular periods
        for (Size i=2; i<schedule.size()-1; i++) {
            start = end; end = schedule.date(i);
            paymentDate = calendar.adjust(end,paymentAdjustment);
            leg.push_back(boost::shared_ptr<CashFlow>(
                new CMSCoupon(get(nominals,i-1), paymentDate, index,
                              start, end, fixingDays, dayCounter, pricer,
                              get(gearings,i-1,1.0),
                              get(spreads,i-1,0.0),
                              get(caps,i-1,Null<Rate>()),
                              get(floors,i-1,Null<Rate>()),
                              get(meanReversions,i-1,Null<Rate>()),
                              start, end, true)));
        }
        if (schedule.size() > 2) {
            // last period might be short or long
            start = end; end = schedule.date(N-1);
            paymentDate = calendar.adjust(end,paymentAdjustment);
            if (schedule.isRegular(N-1)) {
                leg.push_back(boost::shared_ptr<CashFlow>(
                    new CMSCoupon(get(nominals,N-2), paymentDate, index,
                                  start, end, fixingDays, dayCounter, pricer,
                                  get(gearings,N-2,1.0),
                                  get(spreads,N-2,0.0),
                                  get(caps,N-2,Null<Rate>()),
                                  get(floors,N-2,Null<Rate>()),
                                  get(meanReversions,N-2,Null<Rate>()),
                                  start, end, true)));
            } else {
                Date reference = start + schedule.tenor();
                reference =
                    calendar.adjust(reference,paymentAdjustment);
                leg.push_back(boost::shared_ptr<CashFlow>(
                    new CMSCoupon(get(nominals,N-2), paymentDate, index,
                                  start, end, fixingDays, dayCounter, pricer,
                                  get(gearings,N-2,1.0),
                                  get(spreads,N-2,0.0),
                                  get(caps,N-2,Null<Rate>()),
                                  get(floors,N-2,Null<Rate>()),
                                  get(meanReversions,N-2,Null<Rate>()),
                                  start, reference, true)));
            }
        }

        for (Size i=0; i<leg.size(); i++) {
            const boost::shared_ptr<CMSCoupon> cmsCoupon =
               boost::dynamic_pointer_cast<CMSCoupon>(leg[i]);
            if (cmsCoupon)
                cmsCoupon->setSwaptionVolatility(vol);
            else
                QL_FAIL("unexpected error when casting to CMSCoupon");
        }

        return leg;
    }

}
